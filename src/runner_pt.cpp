#include "runner_pt.h"
#include "util.h"
#include <fstream>

namespace loadl {

enum {
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

	MASTER = 0
};

enum {
	TR_CONTINUE,
	TR_CHECKPOINT,
	TR_TIMEUP,
};

pt_chain_run::pt_chain_run(const pt_chain &chain, int run_id) : id{chain.id}, run_id{run_id} {
	rank_to_pos.resize(chain.params.size());
	weight_ratios.resize(chain.params.size(), -1);
	switch_partners.resize(chain.params.size());

	for(size_t i = 0; i < rank_to_pos.size(); i++) {
		rank_to_pos[i] = i;
	}
}

pt_chain_run pt_chain_run::checkpoint_read(const pt_chain &chain, const iodump::group &g) {
	pt_chain_run run;
	g.read("id", run.id);
	assert(chain.id == run.id); 
	g.read("run_id", run.run_id);
	uint8_t swap_odd;
	g.read("swap_odd", swap_odd);
	run.swap_odd = swap_odd;
	
	size_t size = chain.params.size();
	run.weight_ratios.resize(size, -1);
	run.switch_partners.resize(size);
	run.rank_to_pos.resize(size);

	for(size_t i = 0; i < run.rank_to_pos.size(); i++) {
		run.rank_to_pos[i] = i;
	}

	return run;
}

void pt_chain_run::checkpoint_write(const iodump::group &g) {
	g.write("id", id);
	g.write("run_id", run_id);
	g.write("swap_odd", static_cast<uint8_t>(swap_odd));
}

void pt_chain::checkpoint_read(const iodump::group &g) {
	g.read("params", params);
	g.read("rejection_rates", rejection_rates);
	g.read("rejection_rate_entries", rejection_rate_entries);
	g.read("entries_before_optimization", entries_before_optimization);
}

void pt_chain::checkpoint_write(const iodump::group &g) {
	g.write("params", params);
	g.write("rejection_rates", rejection_rates);
	g.write("rejection_rate_entries", rejection_rate_entries);
	g.write("entries_before_optimization", entries_before_optimization);
}

void pt_chain::clear_histograms() {
	rejection_rate_entries[0] = 0;
	rejection_rate_entries[1] = 0;
	std::fill(rejection_rates.begin(), rejection_rates.end(), 0);
}

// https://arxiv.org/pdf/1905.02939.pdf
std::tuple<double, double> pt_chain::optimize_params() {
	std::vector<double> rejection_est(rejection_rates);
	bool odd = false;
	for(auto &r : rejection_est) {
		r /= rejection_rate_entries[odd];
		odd = !odd;
		if(r == 0) { // ensure the comm_barrier is invertible
			r = 1e-3;
		}
	}

	std::vector<double> comm_barrier(params.size());

	double sum{};
	double efficiency{};
	for(size_t i = 0; i < comm_barrier.size() - 1; i++) {
		comm_barrier[i] = sum;
		sum += rejection_est[i];
		efficiency += rejection_est[i] / (1 - rejection_est[i]);
	}
	comm_barrier[comm_barrier.size() - 1] = sum;

	monotonic_interpolator lambda{comm_barrier, params};
	double convergence{};

	for(size_t i = 1; i < params.size() - 1; i++) {
		double new_param = lambda(sum * i / (params.size() - 1));
		double d = (new_param - params[i]);

		convergence += d * d;
		params[i] = new_param;
	}

	convergence = sqrt(convergence) / params.size();

	double round_trip_rate = (1 + sum) / (1 + efficiency);

	return std::tie(round_trip_rate, convergence);
}

bool pt_chain::is_done() {
	return sweeps >= target_sweeps;
}

int runner_pt_start(jobinfo job, const mc_factory &mccreator, int argc, char **argv) {
	MPI_Init(&argc, &argv);

	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	int rc = 0;

	if(rank == 0) {
		runner_pt_master r{std::move(job)};
		rc = r.start();
	} else {
		runner_pt_slave r{std::move(job), mccreator};
		r.start();
	}

	MPI_Finalize();

	return rc;
}

runner_pt_master::runner_pt_master(jobinfo job) : job_{std::move(job)} {
	rng_ = std::make_unique<random_number_generator>();
}

void runner_pt_master::construct_pt_chains() {
	std::string pt_param =
	    job_.jobfile["jobconfig"].get<std::string>("parallel_tempering_parameter");

	for(size_t i = 0; i < job_.task_names.size(); i++) {
		auto task = job_.jobfile["tasks"][job_.task_names[i]];

		auto [chain_id, chain_pos] = task.get<std::pair<int, int>>("pt_chain");
		if(chain_id < 0 || chain_pos < 0) {
			throw std::runtime_error{"parallel tempering pt_chain indices must be nonnegative"};
		}

		if(chain_id >= static_cast<int>(pt_chains_.size())) {
			pt_chains_.resize(chain_id + 1);
		}

		auto &chain = pt_chains_.at(chain_id);
		chain.id = chain_id;

		if(chain_pos >= static_cast<int>(chain.task_ids.size())) {
			chain.task_ids.resize(chain_pos + 1, -1);
			chain.params.resize(chain_pos + 1);
		}

		if(chain.task_ids.at(chain_pos) != -1) {
			throw std::runtime_error{"parallel tempering pt_chain map not unique"};
		}

		chain.params.at(chain_pos) = task.get<double>(pt_param);
		chain.task_ids.at(chain_pos) = i;

		const char *pt_sweep_error =
		    "chain {}: task {}: in parallel tempering mode, sweeps are measured in global updates "
		    "and need to be the "
		    "same within each chain: {} = {} != {}";

		int64_t target_sweeps = task.get<int>("sweeps");
		if(chain.target_sweeps >= 0 && target_sweeps != chain.target_sweeps) {
			throw std::runtime_error{fmt::format(pt_sweep_error, chain.id, i, "target_sweeps",
			                                     chain.target_sweeps, target_sweeps)};
		}
		chain.target_sweeps = target_sweeps;

		int target_thermalization = task.get<int>("thermalization");
		if(chain.target_thermalization >= 0 &&
		   target_thermalization != chain.target_thermalization) {
			throw std::runtime_error{fmt::format(pt_sweep_error, chain.id, i, "thermalization",
			                                     chain.target_thermalization,
			                                     target_thermalization)};
		}
		chain.target_thermalization = target_thermalization;

		int64_t sweeps_per_global_update = task.get<int>("pt_sweeps_per_global_update");
		int64_t sweeps = job_.read_dump_progress(i) / sweeps_per_global_update;
		if(chain.sweeps >= 0 && sweeps != chain.sweeps) {
			throw std::runtime_error{
			    fmt::format(pt_sweep_error, chain.id, i, "sweeps", chain.sweeps, sweeps)};
		}
		chain.sweeps = sweeps;
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

		c.rejection_rates.resize(chain_len_ - 1);

		if(po_config_.enabled) {
			c.entries_before_optimization = po_config_.nsamples_initial;
		}
	}
	if(chain_len_ == -1) {
		throw std::runtime_error{
		    "parallel tempering pt_chain mapping missing. You need to specify 'pt_chain: "
		    "[chain_id, chain_position]' for every task in the job."};
	}

	if((num_active_ranks_ - 1) % chain_len_ != 0) {
		throw std::runtime_error{
		    "parallel tempering: number of ranks should be of the form chain_length*n+1 for best "
		    "efficiency"};
	}
}

void runner_pt_master::checkpoint_read() {
	construct_pt_chains();

	std::string master_dump_name = job_.jobdir() + "/pt_master.dump.h5";
	if(file_exists(master_dump_name)) {
		job_.log(fmt::format("master reading dump from '{}'", master_dump_name));
		iodump dump = iodump::open_readonly(master_dump_name);
		auto g = dump.get_root();

		rng_->checkpoint_read(g.open_group("random_number_generator"));

		auto pt_chains = g.open_group("pt_chains");
		for(std::string name : pt_chains) {
			int id = std::stoi(name);
			pt_chains_.at(id).checkpoint_read(pt_chains.open_group(name));
		}
		
		auto pt_chain_runs = g.open_group("pt_chain_runs");
		for(std::string name : pt_chain_runs) {
			int id;
			pt_chain_runs.read(fmt::format("{}/id", name), id);
			pt_chain_runs_.emplace_back(
			    pt_chain_run::checkpoint_read(pt_chains_.at(id), pt_chain_runs.open_group(name)));
		}
	}
}

void runner_pt_master::write_params_json() {
	nlohmann::json params;
	for(auto c : pt_chains_) {
		params[fmt::format("chain{:04d}", c.id)] = c.params;
	}

	std::ofstream file{job_.jobdir() + "/pt_optimized_params.json"};
	file << params.dump(1) << "\n";
}

void runner_pt_master::write_statistics(const pt_chain_run &chain_run) {
	std::string stat_name = job_.jobdir() + "/pt_statistics.h5";
	iodump stat = iodump::open_readwrite(stat_name);
	auto g = stat.get_root();

	g.write("chain_length", chain_len_);

	auto cg = g.open_group(fmt::format("chain{:04d}_run{:04d}", chain_run.id, chain_run.run_id));
	cg.insert_back("rank_to_pos", chain_run.rank_to_pos);
}

void runner_pt_master::write_param_optimization_statistics() {
	std::string stat_name = job_.jobdir() + "/pt_statistics.h5";
	iodump stat = iodump::open_readwrite(stat_name);
	auto g = stat.get_root();

	g.write("chain_length", chain_len_);

	for(auto &chain : pt_chains_) {
		auto cg = g.open_group(fmt::format("chain{:04d}", chain.id));
		cg.insert_back("params", chain.params);

		std::vector<double> rejection_est(chain.rejection_rates);
		bool odd = false;
		for(auto &r : rejection_est) {
			r /= chain.rejection_rate_entries[odd];
			odd = !odd;
		}

		cg.insert_back("rejection_rates", rejection_est);
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
		c.checkpoint_write(
		    pt_chain_runs.open_group(fmt::format("chain{:04d}_run{:04d}", c.id, c.run_id)));
	}

	auto pt_chains = g.open_group("pt_chains");
	for(auto &c : pt_chains_) {
		c.checkpoint_write(pt_chains.open_group(fmt::format("{:04d}", c.id)));
	}

	if(po_config_.enabled) {
		write_params_json();
	}
}

int runner_pt_master::start() {
	MPI_Comm_size(MPI_COMM_WORLD, &num_active_ranks_);

	po_config_.enabled = job_.jobfile["jobconfig"].defined("pt_parameter_optimization");
	if(po_config_.enabled) {
		job_.log("using feedback parameter optimization");
		const auto &po = job_.jobfile["jobconfig"]["pt_parameter_optimization"];
		po_config_.nsamples_initial = po.get<int>("nsamples_initial");
		po_config_.nsamples_growth = po.get<double>("nsamples_growth");
	}

	job_.log(fmt::format("starting job '{}' in parallel tempering mode", job_.jobname));
	checkpoint_read();

	std::vector<int> group_idx(num_active_ranks_);
	for(int i = 1; i < num_active_ranks_; i++) {
		group_idx[i] = (i - 1) / chain_len_;
	}
	MPI_Scatter(group_idx.data(), 1, MPI_INT, MPI_IN_PLACE, 1, MPI_INT, MASTER, MPI_COMM_WORLD);

	MPI_Comm tmp;
	MPI_Comm_split(MPI_COMM_WORLD, MPI_UNDEFINED, 0, &tmp);

	int chain_count = (num_active_ranks_ - 1) / chain_len_;
	for(int rank_section = 0; rank_section < chain_count; rank_section++) {
		assign_new_chain(rank_section);
	}

	time_last_checkpoint_ = MPI_Wtime();

	while(num_active_ranks_ > 1) {
		react();
		if(MPI_Wtime() - time_last_checkpoint_ > job_.checkpoint_time) {
			time_last_checkpoint_ = MPI_Wtime();
			checkpoint_write();
		}
	}
	checkpoint_write();

	bool all_done = true;
	for(auto &c : pt_chains_) {
		if(!c.is_done()) {
			all_done = false;
			break;
		}
	}
	return !all_done;
}

int runner_pt_master::schedule_chain_run() {
	int old_id = current_chain_id_;
	int nchains = pt_chains_.size();
	for(int i = 1; i <= nchains; i++) {
		if(!pt_chains_[(old_id + i) % nchains].is_done()) {
			current_chain_id_ = (old_id + i) % nchains;
			auto &chain = pt_chains_[current_chain_id_];
			chain.scheduled_runs++;

			int idx = 0;
			for(auto &run : pt_chain_runs_) {
				if(run.id == chain.id && run.run_id == chain.scheduled_runs) {
					return idx;
				}
				idx++;
			}

			pt_chain_runs_.emplace_back(chain, chain.scheduled_runs);
			return pt_chain_runs_.size() - 1;
		}
	}

	// everything done!
	return -1;
}

int runner_pt_master::assign_new_chain(int rank_section) {
	int chain_run_id = schedule_chain_run();
	for(int target = 0; target < chain_len_; target++) {
		int64_t msg[3] = {-1, 0, 0};
		if(chain_run_id >= 0) {
			auto &chain_run = pt_chain_runs_[chain_run_id];
			auto &chain = pt_chains_[chain_run.id];
			msg[0] = chain.task_ids[target];
			msg[1] = chain_run.run_id;
			msg[2] = std::max(1L, chain.target_sweeps - chain.sweeps);
		} else {
			// this will prompt the slave to quit
			num_active_ranks_--;
		}
		MPI_Send(&msg, sizeof(msg) / sizeof(msg[0]), MPI_INT64_T,
		         rank_section * chain_len_ + target + 1, 0, MPI_COMM_WORLD);
	}
	rank_to_chain_run_[rank_section] = chain_run_id;
	return chain_run_id;
}

void runner_pt_master::pt_param_optimization(pt_chain &chain) {
	if(std::min(chain.rejection_rate_entries[0], chain.rejection_rate_entries[1]) >=
	   chain.entries_before_optimization) {
		chain.entries_before_optimization *= po_config_.nsamples_growth;

		auto [efficiency, convergence] = chain.optimize_params();
		job_.log(
		    fmt::format("chain {}: pt param optimization: entries={}, efficiency={:.2g}, "
		                "convergence={:.2g}",
		                chain.id, chain.rejection_rate_entries[0], efficiency, convergence));
		checkpoint_write();
		write_param_optimization_statistics();
		chain.clear_histograms();
	}
}

void runner_pt_master::react() {
	int rank_status;
	MPI_Status stat;
	MPI_Recv(&rank_status, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &stat);
	int rank = stat.MPI_SOURCE - 1;
	if(rank_status == S_BUSY) {
		int64_t msg[1];
		MPI_Recv(msg, sizeof(msg) / sizeof(msg[0]), MPI_INT64_T, rank + 1, 0, MPI_COMM_WORLD,
		         &stat);
		int64_t completed_sweeps = msg[0];

		int chain_run_id = rank_to_chain_run_[rank / chain_len_];
		auto &chain_run = pt_chain_runs_[chain_run_id];
		auto &chain = pt_chains_[chain_run.id];

		chain.sweeps += completed_sweeps;
		if(chain.is_done()) {
			chain.scheduled_runs--;
			int action = A_NEW_JOB;

			if(chain.scheduled_runs > 0) {
				job_.log(fmt::format("chain {} has enough sweeps. Waiting for {} busy rank sets.",
				                     chain.id, chain.scheduled_runs));
			} else {
				job_.log(fmt::format("chain {} rank {} is done. Merging.", chain.id, rank + 1));
				action = A_PROCESS_DATA_NEW_JOB;
				checkpoint_write();
			}
			for(int target = 0; target < chain_len_; target++) {
				send_action(action, rank / chain_len_ * chain_len_ + target + 1);
			}
			assign_new_chain(rank / chain_len_);
		} else {
			for(int target = 0; target < chain_len_; target++) {
				send_action(A_CONTINUE, rank / chain_len_ * chain_len_ + target + 1);
			}
		}
	} else if(rank_status == S_READY_FOR_GLOBAL) {
		int chain_run_id = rank_to_chain_run_[rank / chain_len_];
		auto &chain_run = pt_chain_runs_[chain_run_id];
		auto &chain = pt_chains_[chain_run.id];
		int rank_section = rank / chain_len_;

		if(po_config_.enabled) {
			pt_param_optimization(chain);
		}

		std::fill(chain_run.weight_ratios.begin(), chain_run.weight_ratios.end(), -1);
		for(int target = 0; target < chain_len_; target++) {
			int target_rank = rank_section * chain_len_ + target + 1;
			int pos = chain_run.rank_to_pos[target];
			// keep consistent with pt_global_update
			int partner_pos = pos + (2 * (pos & 1) - 1) * (2 * chain_run.swap_odd - 1);
			if(partner_pos < 0 || partner_pos >= chain_len_) {
				int response = GA_SKIP;
				MPI_Send(&response, 1, MPI_INT, target_rank, 0, MPI_COMM_WORLD);
				chain_run.weight_ratios[pos] = 1;
			} else {
				int response = GA_CALC_WEIGHT;
				MPI_Send(&response, 1, MPI_INT, target_rank, 0, MPI_COMM_WORLD);

				double partner_param = chain.params[partner_pos];
				MPI_Send(&partner_param, 1, MPI_DOUBLE, target_rank, 0, MPI_COMM_WORLD);
			}
		}

		for(int target = 0; target < chain_len_; target++) {
			int target_rank = rank_section * chain_len_ + target + 1;
			int pos = chain_run.rank_to_pos[target];
			if(chain_run.weight_ratios[pos] < 0) {
				double weight;
				MPI_Recv(&weight, 1, MPI_DOUBLE, target_rank, 0, MPI_COMM_WORLD, &stat);
				chain_run.weight_ratios[pos] = weight;
			}
		}

		pt_global_update(chain, chain_run);

		for(int target = 0; target < chain_len_; target++) {
			int new_task_id = chain.task_ids[chain_run.rank_to_pos[target]];
			int partner_rank = rank_section * chain_len_ + chain_run.switch_partners[target] + 1;
			int msg[2] = {new_task_id, partner_rank};
			MPI_Send(msg, sizeof(msg) / sizeof(msg[0]), MPI_INT,
			         rank_section * chain_len_ + target + 1, 0, MPI_COMM_WORLD);
			double new_param = chain.params[chain_run.rank_to_pos[target]];
			MPI_Send(&new_param, 1, MPI_DOUBLE, rank_section * chain_len_ + target + 1, 0,
			         MPI_COMM_WORLD);
		}

		if(job_.jobfile["jobconfig"].get<bool>("pt_statistics", false)) {
			write_statistics(chain_run);
		}
	} else { // S_TIMEUP
		num_active_ranks_--;
	}
}

void runner_pt_master::pt_global_update(pt_chain &chain, pt_chain_run &chain_run) {
	int i = 0;
	for(auto &p : chain_run.switch_partners) {
		p = i++;
	}
	for(int i = chain_run.swap_odd; i < static_cast<int>(chain.task_ids.size()) - 1; i += 2) {
		double w1 = chain_run.weight_ratios[i];
		double w2 = chain_run.weight_ratios[i + 1];

		double p = std::min(exp(w1 + w2), 1.);
		double r = rng_->random_double();

		chain.rejection_rates[i] += 1 - p;
		if(r < p) {
			int rank0{};
			int rank1{};

			int rank = 0;
			for(auto &p : chain_run.rank_to_pos) {
				if(p == i) {
					rank0 = rank;
				} else if(p == i + 1) {
					rank1 = rank;
				}
				rank++;
			}
			chain_run.rank_to_pos[rank0] = i + 1;
			chain_run.rank_to_pos[rank1] = i;

			chain_run.switch_partners[rank0] = rank1;
			chain_run.switch_partners[rank1] = rank0;
		}
	}

	chain.rejection_rate_entries[chain_run.swap_odd]++;
	chain_run.swap_odd = !chain_run.swap_odd;
}

runner_pt_slave::runner_pt_slave(jobinfo job, mc_factory mccreator)
    : job_{std::move(job)}, mccreator_{std::move(mccreator)} {}

void runner_pt_slave::start() {
	MPI_Comm_rank(MPI_COMM_WORLD, &rank_);
	time_start_ = MPI_Wtime();
	time_last_checkpoint_ = time_start_;

	int group_idx;
	MPI_Scatter(NULL, 1, MPI_INT, &group_idx, 1, MPI_INT, MASTER, MPI_COMM_WORLD);
	MPI_Comm_split(MPI_COMM_WORLD, group_idx, 0, &chain_comm_);

	MPI_Comm_rank(chain_comm_, &chain_rank_);

	bool use_param_optimization = job_.jobfile["jobconfig"].defined("pt_parameter_optimization");

	if(!accept_new_chain()) {
		job_.log(fmt::format("rank {} exits: out of work", rank_));
		return;
	}

	int action{};
	do {
		int timeout{TR_CHECKPOINT};

		while(sweeps_since_last_query_ < sweeps_before_communication_) {
			sys_->_do_update();

			if(sys_->is_thermalized() && !use_param_optimization) {
				sys_->_do_measurement();
			}

			if(sys_->sweep() % sweeps_per_global_update_ == 0) {
				pt_global_update();
				if(sys_->is_thermalized()) {
					sweeps_since_last_query_++;
				}

				timeout = negotiate_timeout();
				if(timeout != TR_CONTINUE) {
					break;
				}
			}
		}

		checkpoint_write();

		if(timeout == TR_TIMEUP) {
			send_status(S_TIMEUP);
			job_.log(fmt::format("rank {} exits: time up", rank_));
			break;
		}
		action = what_is_next(S_BUSY);
	} while(action != A_EXIT);

	if(action == A_EXIT) {
		job_.log(fmt::format("rank {} exits: out of work", rank_));
	}
}

void runner_pt_slave::send_status(int status) {
	MPI_Send(&status, 1, MPI_INT, MASTER, 0, MPI_COMM_WORLD);
}

int runner_pt_slave::negotiate_timeout() {
	int result = TR_CONTINUE;
	if(chain_rank_ == 0) {
		if(MPI_Wtime() - time_last_checkpoint_ > job_.checkpoint_time) {
			result = TR_CHECKPOINT;
		}

		if(MPI_Wtime() - time_start_ > job_.runtime) {
			result = TR_TIMEUP;
		}
	}

	MPI_Bcast(&result, 1, MPI_INT, 0, chain_comm_);
	return result;
}

void runner_pt_slave::pt_global_update() {
	if(chain_rank_ == 0) {
		send_status(S_READY_FOR_GLOBAL);
	}

	int response;
	MPI_Recv(&response, 1, MPI_INT, MASTER, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	// job_.log(fmt::format(" * rank {}: ready for global update", rank_));

	const std::string &param_name =
	    job_.jobfile["jobconfig"].get<std::string>("parallel_tempering_parameter");

	if(response == GA_CALC_WEIGHT) {
		double partner_param;
		MPI_Recv(&partner_param, 1, MPI_DOUBLE, MASTER, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		double weight_ratio = sys_->_pt_weight_ratio(param_name, partner_param);
		MPI_Send(&weight_ratio, 1, MPI_DOUBLE, MASTER, 0, MPI_COMM_WORLD);
		// job_.log(fmt::format(" * rank {}: weight sent", rank_));
	} else {
		// job_.log(fmt::format(" * rank {}: no weight needed", rank_));
	}
	MPI_Barrier(chain_comm_);

	// this may be a long wait

	int msg[2];
	MPI_Recv(&msg, sizeof(msg) / sizeof(msg[0]), MPI_INT, MASTER, 0, MPI_COMM_WORLD,
	         MPI_STATUS_IGNORE);
	int new_task_id = msg[0];
	int target_rank = msg[1];

	double new_param;
	MPI_Recv(&new_param, 1, MPI_DOUBLE, MASTER, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

	if(new_task_id != task_id_ || current_param_ != new_param) {
		task_id_ = new_task_id;
		sweeps_per_global_update_ = job_.jobfile["tasks"][job_.task_names[task_id_]].get<int>(
		    "pt_sweeps_per_global_update");
		sys_->_pt_update_param(target_rank, param_name, new_param);
	}
	current_param_ = new_param;
}

bool runner_pt_slave::accept_new_chain() {
	int64_t msg[3];
	MPI_Recv(&msg, sizeof(msg) / sizeof(msg[0]), MPI_INT64_T, 0, 0, MPI_COMM_WORLD,
	         MPI_STATUS_IGNORE);
	task_id_ = msg[0];
	run_id_ = msg[1];
	sweeps_before_communication_ = msg[2];

	if(task_id_ < 0) {
		return false;
	}

	sweeps_per_global_update_ = job_.jobfile["tasks"][job_.task_names[task_id_]].get<int64_t>(
	    "pt_sweeps_per_global_update");

	sys_ = std::unique_ptr<mc>{mccreator_(job_.jobfile["tasks"][job_.task_names[task_id_]])};
	sys_->pt_mode_ = true;
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
	if(chain_rank_ == 0) {
		send_status(status);

		int64_t msg[1] = {sweeps_since_last_query_};
		MPI_Send(msg, sizeof(msg) / sizeof(msg[0]), MPI_INT64_T, 0, 0, MPI_COMM_WORLD);
	}
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
	MPI_Barrier(chain_comm_);
	sys_->_write_finalize(job_.rundir(task_id_, run_id_));
	job_.log(fmt::format("* rank {}: checkpoint {}", rank_, job_.rundir(task_id_, run_id_)));
}

void runner_pt_master::send_action(int action, int destination) {
	MPI_Send(&action, 1, MPI_INT, destination, 0, MPI_COMM_WORLD);
}

int runner_pt_slave::recv_action() {
	int new_action;
	MPI_Recv(&new_action, 1, MPI_INT, MASTER, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	return new_action;
}

void runner_pt_slave::merge_measurements() {
	std::string unique_filename = job_.taskdir(task_id_);
	sys_->write_output(unique_filename);

	std::vector<evalable> evalables;
	if(job_.jobfile["jobconfig"].defined("pt_parameter_optimization")) {
		if(rank_ == 1) {
			job_.log("Running in parameter optimization mode. No evalables are calculated.");
		}
	} else {
		sys_->register_evalables(evalables);
	}
	job_.merge_task(task_id_, evalables);
}

}
