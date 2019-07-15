#include "runner_pt.h"

namespace loadl {

enum {
	S_IDLE,
	S_BUSY,
	S_READY_FOR_GLOBAL,
	S_TIMEUP,

	A_CONTINUE,
	A_EXIT,
	A_NEW_JOB,
	A_PROCESS_DATA_NEW_JOB,

	// global update status response
	GA_DONE,
	GA_SKIP,
	GA_CALC_WEIGHT,

	T_ACTION,
	T_GLOBAL,
	T_NEW_JOB,
	T_STATUS,

	MASTER=0
};

pt_chain_run::pt_chain_run(const pt_chain& chain, int run_id) 
	: id{chain.id}, run_id{run_id}, params{chain.start_params} {
	node_to_pos.resize(params.size());
	done.resize(params.size());
	weight_ratios.resize(params.size());

	for(size_t i = 0; i < node_to_pos.size(); i++) {
		node_to_pos[i] = i;
	}
}

pt_chain_run pt_chain_run::checkpoint_read(const iodump::group& g) {
	pt_chain_run run;
	g.read("id", run.id);
	g.read("run_id", run.run_id);
	g.read("params", run.params);
	g.read("node_to_pos", run.node_to_pos);

	run.weight_ratios.resize(run.params.size());
	run.done.resize(run.params.size());

	return run;
}

void pt_chain_run::checkpoint_write(const iodump::group& g) {
	g.write("id", id);
	g.write("run_id", run_id);
	g.write("node_to_pos", node_to_pos);
	g.write("params", params);
}

bool pt_chain::is_done() {
	return std::all_of(sweeps.begin(), sweeps.end(), [this](double s) { return s >= target_sweeps + target_thermalization;});
}

int runner_pt_start(jobinfo job, const mc_factory &mccreator, int argc, char **argv) {
	MPI_Init(&argc, &argv);

	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	if(rank == 0) {
		runner_pt_master r{std::move(job)};
		r.start();
	} else {
		runner_pt_slave r{std::move(job), mccreator};
		r.start();
	}

	MPI_Finalize();

	return 0;
}


runner_pt_master::runner_pt_master(jobinfo job) : job_{std::move(job)} {
	rng_ = std::make_unique<random_number_generator>();
}

void runner_pt_master::construct_pt_chains() {
	std::string pt_param = job_.jobfile["jobconfig"].get<std::string>("parallel_tempering_parameter");
	
	for(size_t i = 0; i < job_.task_names.size(); i++) {
		auto task = job_.jobfile["tasks"][job_.task_names[i]];

		auto [chain_id, chain_pos] = task.get<std::pair<int, int>>("pt_chain");
		if(chain_id < 0 || chain_pos < 0) {
			throw std::runtime_error{"parallel tempering pt_chain indices must be nonnegative"};
		}

		if(chain_id >= static_cast<int>(pt_chains_.size())) {
			pt_chains_.resize(chain_id+1);
		}

		auto &chain = pt_chains_.at(chain_id);
		chain.id = chain_id;

		if(chain_pos >= static_cast<int>(chain.task_ids.size())) {
			chain.task_ids.resize(chain_pos+1, -1);
			chain.start_params.resize(chain_pos+1);
			chain.sweeps.resize(chain_pos+1);
		}
		
		if(chain.task_ids.at(chain_pos) != -1) {
			throw std::runtime_error{"parallel tempering pt_chain map not unique"};
		}

		chain.start_params.at(chain_pos) = task.get<double>(pt_param);
		chain.task_ids.at(chain_pos) = i;
		chain.target_sweeps = std::max(chain.target_sweeps, task.get<int>("sweeps"));
		chain.target_thermalization = std::max(chain.target_thermalization, task.get<int>("thermalization"));

		chain.sweeps[chain_pos] = job_.read_dump_progress(chain_pos);
	}

	chain_len_ = -1;
	for(auto &c : pt_chains_) {
		if(c.id == -1) {
			throw std::runtime_error{"parallel tempering pt_chain map contains gaps"};
		}
		
		if(chain_len_ != -1 && chain_len_ != static_cast<int>(c.task_ids.size())) {
			throw std::runtime_error{"parallel tempering pt_chains do not have the same length"};
		}
		chain_len_ = c.task_ids.size();
	}
	if(chain_len_ == -1) {
		throw std::runtime_error{"parallel tempering pt_chain mapping missing. You need to specify 'pt_chain: [chain_id, chain_position]' for every task in the job."};
	}

	if((num_active_ranks_-1) % chain_len_ != 0) {
		throw std::runtime_error{"parallel tempering: number of ranks should be of the form chain_length*n+1 for best efficiency"};
	}
}

void runner_pt_master::checkpoint_read() {
	construct_pt_chains();

	std::string master_dump_name = job_.jobdir() + "/pt_master.dump.h5";
	if(file_exists(master_dump_name)) {		
		iodump dump = iodump::open_readonly(master_dump_name);
		auto g = dump.get_root();

		rng_->checkpoint_read(g.open_group("random_number_generator"));
		auto pt_chain_runs = g.open_group("pt_chain_runs");
		for(std::string name : pt_chain_runs) {
			pt_chain_runs_.emplace_back(pt_chain_run::checkpoint_read(pt_chain_runs.open_group(name)));
		}

		g.read("current_chain_id", current_chain_id_);
		uint8_t pt_swap_odd;
		g.read("pt_swap_odd", pt_swap_odd);
		pt_swap_odd_ = pt_swap_odd;
	}
}

void runner_pt_master::checkpoint_write() {
	std::string master_dump_name = job_.jobdir() + "/pt_master.dump.h5";

	job_.log(fmt::format("master: checkpoint {}", master_dump_name));
	
	iodump dump = iodump::create(master_dump_name);
	auto g = dump.get_root();

	rng_->checkpoint_write(g.open_group("random_number_generator"));
	auto pt_chain_runs = g.open_group("pt_chain_runs");
	for(auto &c : pt_chain_runs_) {
		c.checkpoint_write(pt_chain_runs.open_group(fmt::format("chain{:04d}_run{:04d}", c.id, c.run_id)));
	}

	g.write("current_chain_id", current_chain_id_);
	g.write("pt_swap_odd", static_cast<uint8_t>(pt_swap_odd_));
}

void runner_pt_master::start() {
	MPI_Comm_size(MPI_COMM_WORLD, &num_active_ranks_);

	job_.log(fmt::format("Starting job '{}'", job_.jobname));
	checkpoint_read();

	for(int node_section = 0; node_section < (num_active_ranks_-1)/chain_len_; node_section++) {
		assign_new_chain(node_section);
	}

	time_last_checkpoint_ = MPI_Wtime();
	
	while(num_active_ranks_ > 1) {
		react();
		if(MPI_Wtime() - time_last_checkpoint_ > job_.checkpoint_time) {
			time_last_checkpoint_ = MPI_Wtime();
			checkpoint_write();
		}
	}
}

int runner_pt_master::schedule_chain_run() {
	int old_id = current_chain_id_;
	int nchains = pt_chains_.size();
	for(int i = 1; i <= nchains; i++) {
		if(!pt_chains_[(old_id + i) % nchains].is_done()) {
			int new_chain_id = (old_id + i) % nchains;
			auto &chain = pt_chains_[new_chain_id];
			chain.scheduled_runs++;
			
			int idx = 0;
			for(auto &run : pt_chain_runs_) {
				if(run.id == chain.id && run.run_id == chain.scheduled_runs) {
					return idx;
				}
				idx++;
			}

			pt_chain_runs_.emplace_back(chain, chain.scheduled_runs);
			return pt_chain_runs_.size()-1;
		}
	}

	// everything done!
	return -1;
}

int runner_pt_master::assign_new_chain(int node_section) {
	int chain_run_id = schedule_chain_run();

	for(int target = 0; target < chain_len_; target++) {
		int msg[3] = {-1,0,0};
		if(chain_run_id >= 0) {
			auto &chain_run = pt_chain_runs_[chain_run_id];
			auto &chain = pt_chains_[chain_run.id];
			msg[0] = chain.task_ids[target];
			msg[1] = chain_run.run_id;
			msg[2] = chain.target_sweeps+chain.target_thermalization-chain.sweeps[chain_run.node_to_pos[target]];
		} else {
			// this will prompt the slave to quit
			num_active_ranks_--;
		}
		MPI_Send(&msg, sizeof(msg) / sizeof(msg[0]), MPI_INT, node_section*chain_len_+target+1, T_NEW_JOB,
	         MPI_COMM_WORLD);
	}
	node_to_chain_run_[node_section] = chain_run_id;
	return chain_run_id;
}

void runner_pt_master::react() {
	int node_status;
	MPI_Status stat;
	MPI_Recv(&node_status, 1, MPI_INT, MPI_ANY_SOURCE, T_STATUS, MPI_COMM_WORLD, &stat);
	int node = stat.MPI_SOURCE-1;
	if(node_status == S_BUSY) {
		int msg[1];
		MPI_Recv(msg, sizeof(msg) / sizeof(msg[0]), MPI_INT, node+1, T_STATUS, MPI_COMM_WORLD, &stat);
		int completed_sweeps = msg[0];

		int chain_run_id = node_to_chain_run_[node/chain_len_];
		auto &chain_run = pt_chain_runs_[chain_run_id];
		auto &chain = pt_chains_[chain_run.id];

		chain.sweeps[chain_run.node_to_pos[node%chain_len_]] += completed_sweeps;

		if(chain.is_done()) {
			chain_run.done[node%chain_len_] = true;

			if(std::all_of(chain_run.done.begin(), chain_run.done.end(), [](bool x) {return x;})) {
				chain.scheduled_runs--;
				int action = A_NEW_JOB;
				
				if(chain.scheduled_runs > 0) {
					job_.log(fmt::format("chain {} has enough sweeps. Waiting for {} busy rank sets.", chain.id, chain.scheduled_runs));
				} else {
					job_.log(fmt::format("chain {} rank {} is done. Merging.", chain.id, node+1));
					action = A_PROCESS_DATA_NEW_JOB;
					checkpoint_write();
				}
				for(int target = 0; target < chain_len_; target++) {
					send_action(action, node/chain_len_*chain_len_+target+1);
				}
				std::fill(chain_run.done.begin(), chain_run.done.end(), -1);
				assign_new_chain(node/chain_len_);
			}
		} else {
			send_action(A_CONTINUE, node+1);
		}
	} else if(node_status == S_READY_FOR_GLOBAL) {
		int chain_run_id = node_to_chain_run_[node/chain_len_];
		auto &chain_run = pt_chain_runs_[chain_run_id];
		auto &chain = pt_chains_[chain_run.id];

		if(chain.is_done()) {
			int exit = GA_DONE;
			MPI_Send(&exit, 1, MPI_INT, node+1, T_GLOBAL, MPI_COMM_WORLD);
		} else {
			int pos = chain_run.node_to_pos[node%chain_len_];
			// keep consistent with pt_global_update
			int partner_pos = pos + (2*(pos&1)-1)*(2*pt_swap_odd_-1);
			if(partner_pos < 0 || partner_pos >= chain_len_) {
				int response = GA_SKIP;
				MPI_Send(&response, 1, MPI_INT, node+1, T_GLOBAL, MPI_COMM_WORLD);
				chain_run.weight_ratios[chain_run.node_to_pos[node%chain_len_]] = 1;
			} else {
				int response = GA_CALC_WEIGHT;
				MPI_Send(&response, 1, MPI_INT, node+1, T_GLOBAL, MPI_COMM_WORLD);

				double partner_param = chain_run.params[partner_pos];
				MPI_Send(&partner_param, 1, MPI_DOUBLE, node+1, T_GLOBAL, MPI_COMM_WORLD);

				double weight;
				MPI_Recv(&weight, 1, MPI_DOUBLE, node+1, T_GLOBAL, MPI_COMM_WORLD, &stat);
				chain_run.weight_ratios[chain_run.node_to_pos[node%chain_len_]] = weight;
			}


			bool all_ready = std::all_of(chain_run.weight_ratios.begin(), chain_run.weight_ratios.end(), [](double w) { return w > 0; });
			
			if(all_ready) {
				pt_global_update(chain, chain_run);
				std::fill(chain_run.weight_ratios.begin(), chain_run.weight_ratios.end(), -1);
				int node_section = node/chain_len_;

				for(int target = 0; target < chain_len_; target++) {
					int new_task_id = chain.task_ids[chain_run.node_to_pos[target]];
					double new_param = chain_run.params[chain_run.node_to_pos[target]];
					MPI_Send(&new_task_id, 1, MPI_INT, node_section*chain_len_+target+1, T_GLOBAL, MPI_COMM_WORLD);
					MPI_Send(&new_param, 1, MPI_DOUBLE, node_section*chain_len_+target+1, T_GLOBAL, MPI_COMM_WORLD);
				}
			}
		}
	} else { // S_TIMEUP
		num_active_ranks_--;
	}
}



void runner_pt_master::pt_global_update(pt_chain &chain, pt_chain_run &chain_run) {
	job_.log(fmt::format("global update on chain_run {}:{}", chain_run.id, chain_run.run_id));
	for(int i = pt_swap_odd_; i < static_cast<int>(chain.task_ids.size())-1; i+=2) {
		double w1 = chain_run.weight_ratios[i];
		double w2 = chain_run.weight_ratios[i+1];

		double r = rng_->random_double();

		if(r < w1*w2) {
			std::swap(chain_run.node_to_pos[i], chain_run.node_to_pos[i+1]);
		}
	}

	pt_swap_odd_ = !pt_swap_odd_;
}

runner_pt_slave::runner_pt_slave(jobinfo job, mc_factory mccreator)
    : job_{std::move(job)}, mccreator_{std::move(mccreator)} {}

void runner_pt_slave::start() {
	MPI_Comm_rank(MPI_COMM_WORLD, &rank_);
	time_start_ = MPI_Wtime();
	time_last_checkpoint_ = time_start_;


	if(!accept_new_chain()) {
		return;
	}

	int action{};
	do {
		while(sweeps_since_last_query_ < sweeps_before_communication_) {
			sys_->_do_update();
			sweeps_since_last_query_++;

			if(sys_->is_thermalized()) {
				sys_->_do_measurement();
			}

			if(sys_->sweep() % sweeps_per_global_update_ == 0) {
				pt_global_update();
			}

			if(is_checkpoint_time() || time_is_up()) {
				break;
			}
			
		}
		checkpoint_write();

		if(time_is_up()) {
			send_status(S_TIMEUP);
			job_.log(fmt::format("rank {} exits: walltime up (safety interval = {} s)", rank_, sys_->safe_exit_interval()));
			break;
		}
		action = what_is_next(S_BUSY);
	} while(action != A_EXIT);

	if(action == A_EXIT) {
		job_.log(fmt::format("rank {} exits: out of work", rank_));
	}
}

void runner_pt_slave::send_status(int status) {
	MPI_Send(&status, 1, MPI_INT, MASTER, T_STATUS, MPI_COMM_WORLD);
}

void runner_pt_slave::pt_global_update() {
	MPI_Status stat;
	send_status(S_READY_FOR_GLOBAL);

	int response;
	MPI_Recv(&response, 1, MPI_INT, MASTER, T_GLOBAL, MPI_COMM_WORLD, &stat);

	if(response == GA_DONE) {
		return;
	} else if(response == GA_CALC_WEIGHT) {
		job_.log(fmt::format(" * rank {}: ready for global update", rank_));
		double partner_param;
		MPI_Recv(&partner_param, 1, MPI_DOUBLE, MASTER, T_GLOBAL, MPI_COMM_WORLD, &stat);
		double weight_ratio = sys_->pt_weight_ratio(partner_param);
		MPI_Send(&weight_ratio, 1, MPI_DOUBLE, MASTER, T_GLOBAL, MPI_COMM_WORLD);
	}

	// this may be a long wait

	double new_param;
	MPI_Recv(&task_id_, 1, MPI_INT, MASTER, T_GLOBAL, MPI_COMM_WORLD, &stat);
	MPI_Recv(&new_param, 1, MPI_DOUBLE, MASTER, T_GLOBAL, MPI_COMM_WORLD, &stat);
	sweeps_per_global_update_ = job_.jobfile["tasks"][job_.task_names[task_id_]].get<int>("pt_sweeps_per_global_update");

	sys_->_pt_update_param(new_param, job_.rundir(task_id_, run_id_));
}

bool runner_pt_slave::accept_new_chain() {
	MPI_Status stat;
	int msg[3];
	MPI_Recv(&msg, sizeof(msg) / sizeof(msg[0]), MPI_INT, 0, T_NEW_JOB, MPI_COMM_WORLD, &stat);
	task_id_ = msg[0];
	run_id_ = msg[1];
	sweeps_before_communication_ = msg[2];

	if(task_id_ < 0) {
		return false;
	}

	sweeps_per_global_update_ = job_.jobfile["tasks"][job_.task_names[task_id_]].get<int>("pt_sweeps_per_global_update");

	sys_ =
	    std::unique_ptr<mc>{mccreator_(job_.jobfile["tasks"][job_.task_names[task_id_]])};
	if(!sys_->_read(job_.rundir(task_id_, run_id_))) {
		sys_->_init();
		job_.log(fmt::format("* initialized {}", job_.rundir(task_id_, run_id_)));
		checkpoint_write();
	} else {
		job_.log(fmt::format("* read {}", job_.rundir(task_id_, run_id_)));
	}

	return true;
}

int runner_pt_slave::what_is_next(int status) {
	send_status(status);

	int msg[1] = {sweeps_since_last_query_};
	MPI_Send(msg, sizeof(msg) / sizeof(msg[0]), MPI_INT, 0, T_STATUS, MPI_COMM_WORLD);
	sweeps_since_last_query_ = 0;
	int new_action = recv_action();
	
	if(new_action == A_EXIT) {
		return A_EXIT;
	}

	if(new_action != A_CONTINUE) {
		if(new_action == A_PROCESS_DATA_NEW_JOB) {
			merge_measurements();
		}

		if(!accept_new_chain()) {
			return A_EXIT;
		}
	}
	return A_CONTINUE;
}

void runner_pt_slave::checkpoint_write() {
	time_last_checkpoint_ = MPI_Wtime();
	sys_->_write(job_.rundir(task_id_, run_id_));
	job_.log(fmt::format("* rank {}: checkpoint {}", rank_, job_.rundir(task_id_, run_id_)));
}

void runner_pt_master::send_action(int action, int destination) {
	MPI_Send(&action, 1, MPI_INT, destination, T_ACTION, MPI_COMM_WORLD);
}

int runner_pt_slave::recv_action() {
	MPI_Status stat;
	int new_action;
	MPI_Recv(&new_action, 1, MPI_INT, MASTER, T_ACTION, MPI_COMM_WORLD, &stat);
	return new_action;
}

void runner_pt_slave::merge_measurements() {
	std::string unique_filename = job_.taskdir(task_id_);
	sys_->_write_output(unique_filename);

	std::vector<evalable> evalables;
	sys_->register_evalables(evalables);
	job_.merge_task(task_id_, evalables);
}

bool runner_pt_slave::is_checkpoint_time() {
	return MPI_Wtime() - time_last_checkpoint_ > job_.checkpoint_time;
}

bool runner_pt_slave::time_is_up() {
	double safe_interval = 0;
	if(sys_ != nullptr) {
		safe_interval = sys_->safe_exit_interval();
	}
	return MPI_Wtime() - time_start_ > job_.walltime-safe_interval;
}

}
