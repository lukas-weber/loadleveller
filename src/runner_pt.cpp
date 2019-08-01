#include "runner_pt.h"
#include <fstream>

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

	last_visited.resize(chain.params.size());

	for(size_t i = 0; i < rank_to_pos.size(); i++) {
		rank_to_pos[i] = i;
	}
}

pt_chain_run pt_chain_run::checkpoint_read(const iodump::group &g) {
	pt_chain_run run;
	g.read("id", run.id);
	g.read("run_id", run.run_id);
	g.read("rank_to_pos", run.rank_to_pos);
	g.read("last_visited", run.last_visited);
	uint8_t swap_odd;
	g.read("swap_odd", swap_odd);
	run.swap_odd = swap_odd;

	run.weight_ratios.resize(run.rank_to_pos.size(), -1);
	run.switch_partners.resize(run.rank_to_pos.size());

	return run;
}

void pt_chain_run::checkpoint_write(const iodump::group &g) {
	g.write("id", id);
	g.write("run_id", run_id);
	g.write("rank_to_pos", rank_to_pos);
	g.write("last_visited", last_visited);
	g.write("swap_odd", static_cast<uint8_t>(swap_odd));
}

void pt_chain::checkpoint_read(const iodump::group &g) {
	g.read("params", params);
	g.read("nup_histogram", nup_histogram);
	g.read("ndown_histogram", ndown_histogram);
	g.read("entries_before_optimization", entries_before_optimization);
}

void pt_chain::checkpoint_write(const iodump::group &g) {
	g.write("params", params);
	g.write("nup_histogram", nup_histogram);
	g.write("ndown_histogram", ndown_histogram);
	g.write("entries_before_optimization", entries_before_optimization);
}

int pt_chain::histogram_entries() {
	return nup_histogram.at(0);
}

void pt_chain::clear_histograms() {
	std::fill(nup_histogram.begin(), nup_histogram.end(), 0);
	std::fill(ndown_histogram.begin(), ndown_histogram.end(), 0);
}

static double linear_regression(const std::vector<double> &x, const std::vector<double> &y, int i,
                                int range) {
	int lower = std::max(0, i - range + 1);
	int upper = std::min(static_cast<int>(y.size() - 1), i + range);

	double sxy = 0;
	double sx = 0, sy = 0;
	double sx2 = 0;

	for(int j = lower; j <= upper; j++) {
		sxy += x[j] * y[j];
		sx += x[j];
		sy += y[j];
		sx2 += x[j] * x[j];
	}

	int n = upper - lower + 1;
	return (sxy - sx * sy / n) / (sx2 - sx * sx / n);
}

std::tuple<double, double> pt_chain::optimize_params(int linreg_len) {
	std::vector<double> eta(params.size());
	std::vector<double> f(params.size());
	std::vector<double> new_params(params.size());

	assert(params.size() > 1);

	new_params[0] = params[0];
	new_params[params.size() - 1] = params[params.size() - 1];

	double fnonlinearity = 0;
	// in the worst case, f=0 or f=1 for [1,N-2]
	int n = params.size();
	double fnonlinearity_worst = sqrt((2 * n + 1. / n - 3) / 6.);
	for(size_t i = 0; i < params.size(); i++) {
		if(nup_histogram[i] + ndown_histogram[i] == 0) {
			f[i] = 0;
		} else {
			f[i] = nup_histogram[i] / static_cast<double>(nup_histogram[i] + ndown_histogram[i]);
		}

		double ideal_f = 1 - i / static_cast<double>(params.size());
		fnonlinearity += (f[i] - ideal_f) * (f[i] - ideal_f);
	}
	fnonlinearity = sqrt(fnonlinearity) / fnonlinearity_worst;

	double norm = 0;
	for(size_t i = 0; i < params.size() - 1; i++) {
		double dfdp = linear_regression(params, f, i, linreg_len);
		double dp = params[i + 1] - params[i];
		eta[i] = sqrt(std::max(0.01, -dfdp / dp));
		norm += eta[i] * dp;
	}
	for(auto &v : eta) {
		v /= norm;
	}

	double convergence = 0;
	for(size_t i = 1; i < params.size() - 1; i++) {
		double target = static_cast<double>(i) / (params.size() - 1);
		int etai = 0;
		for(etai = 0; etai < static_cast<int>(params.size()) - 1; etai++) {
			double deta = eta[etai] * (params[etai + 1] - params[etai]);
			if(deta > target) {
				break;
			}
			target -= deta;
		}
		new_params[i] = params[etai] + target / eta[etai];
		convergence += (new_params[i] - params[i]) * (new_params[i] - params[i]);
	}

	convergence = sqrt(convergence) / (params.size() - 2);

	for(size_t i = 0; i < params.size(); i++) {
		double relaxation_fac = 1;
		params[i] = params[i] * (1 - relaxation_fac) + relaxation_fac * new_params[i];
	}

	return std::tie(fnonlinearity, convergence);
}

bool pt_chain::is_done() {
	return sweeps >= target_sweeps + target_thermalization;
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
		    "in parallel tempering mode, sweeps are measured in global updates and need to be the "
		    "same within each chain: {} = {} != {}";

		int target_sweeps = task.get<int>("sweeps");
		if(chain.target_sweeps >= 0 && target_sweeps != chain.target_sweeps) {
			throw std::runtime_error{
			    fmt::format(pt_sweep_error, "target_sweeps", chain.target_sweeps, target_sweeps)};
		}
		chain.target_sweeps = target_sweeps;

		int target_thermalization = task.get<int>("thermalization");
		if(chain.target_thermalization >= 0 &&
		   target_thermalization != chain.target_thermalization) {
			throw std::runtime_error{fmt::format(pt_sweep_error, "thermalization",
			                                     chain.target_thermalization,
			                                     target_thermalization)};
		}
		chain.target_thermalization = target_thermalization;

		int sweeps_per_global_update = task.get<int>("pt_sweeps_per_global_update");
		int sweeps = job_.read_dump_progress(i) / sweeps_per_global_update;
		if(chain.sweeps >= 0 && sweeps != chain.sweeps) {
			throw std::runtime_error{fmt::format(pt_sweep_error, "sweeps", chain.sweeps != sweeps)};
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

		c.nup_histogram.resize(chain_len_);
		c.ndown_histogram.resize(chain_len_);
		c.entries_before_optimization =
		    job_.jobfile["jobconfig"].get<int>("pt_parameter_optimization_nsamples_initial", 10000);
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
		auto pt_chain_runs = g.open_group("pt_chain_runs");
		for(std::string name : pt_chain_runs) {
			pt_chain_runs_.emplace_back(
			    pt_chain_run::checkpoint_read(pt_chain_runs.open_group(name)));
		}

		auto pt_chains = g.open_group("pt_chains");
		for(std::string name : pt_chains) {
			int id = std::stoi(name);
			pt_chains_.at(id).checkpoint_read(pt_chains.open_group(name));
		}
	}
}

void runner_pt_master::write_params_yaml() {
	using namespace YAML;
	Emitter params;
	params << BeginMap;
	for(auto c : pt_chains_) {
		params << Key << fmt::format("chain{:04d}", c.id);
		params << Value << Flow << c.params;
	}
	params << EndMap;

	std::ofstream file{job_.jobdir() + "/pt_optimized_params.yml"};
	file << params.c_str() << "\n";
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

	if(use_param_optimization_) {
		write_params_yaml();
	}
}

void runner_pt_master::start() {
	MPI_Comm_size(MPI_COMM_WORLD, &num_active_ranks_);

	job_.log(fmt::format("starting job '{}' in parallel tempering mode", job_.jobname));
	checkpoint_read();

	use_param_optimization_ =
	    job_.jobfile["jobconfig"].get<bool>("pt_parameter_optimization", false);
	if(use_param_optimization_) {
		job_.log("using feedback parameter optimization");
	}

	std::vector<int> group_idx(num_active_ranks_);
	for(int i = 1; i < num_active_ranks_; i++) {
		group_idx[i] = (i - 1) / chain_len_;
	}
	MPI_Scatter(group_idx.data(), 1, MPI_INT, MPI_IN_PLACE, 1, MPI_INT, MASTER, MPI_COMM_WORLD);

	MPI_Comm tmp;
	MPI_Comm_split(MPI_COMM_WORLD, MPI_UNDEFINED, 0, &tmp);

	for(int rank_section = 0; rank_section < (num_active_ranks_ - 1) / chain_len_; rank_section++) {
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
		int msg[3] = {-1, 0, 0};
		if(chain_run_id >= 0) {
			auto &chain_run = pt_chain_runs_[chain_run_id];
			auto &chain = pt_chains_[chain_run.id];
			msg[0] = chain.task_ids[target];
			msg[1] = chain_run.run_id;
			msg[2] = chain.target_sweeps + chain.target_thermalization - chain.sweeps;
		} else {
			// this will prompt the slave to quit
			num_active_ranks_--;
		}
		MPI_Send(&msg, sizeof(msg) / sizeof(msg[0]), MPI_INT,
		         rank_section * chain_len_ + target + 1, 0, MPI_COMM_WORLD);
	}
	rank_to_chain_run_[rank_section] = chain_run_id;
	return chain_run_id;
}

void runner_pt_master::pt_param_optimization(pt_chain &chain, pt_chain_run &chain_run) {
	for(size_t rank = 0; rank < chain_run.rank_to_pos.size(); rank++) {
		if(chain_run.rank_to_pos[rank] == 0) {
			chain_run.last_visited[rank] = 1;
		}
		if(chain_run.rank_to_pos[rank] == static_cast<int>(chain_run.rank_to_pos.size()) - 1) {
			chain_run.last_visited[rank] = -1;
		}

		chain.ndown_histogram[chain_run.rank_to_pos[rank]] += chain_run.last_visited[rank] == -1;
		chain.nup_histogram[chain_run.rank_to_pos[rank]] += chain_run.last_visited[rank] == 1;
	}
	if(chain.histogram_entries() >= chain.entries_before_optimization) {
		chain.entries_before_optimization *=
		    job_.jobfile["jobconfig"].get<double>("pt_parameter_optimization_nsamples_growth", 1.5);
		if(std::any_of(chain_run.last_visited.begin(), chain_run.last_visited.end(), [](int label) { return label == 0; })) {
			job_.log(fmt::format("chain {}: some ranks still have no label. holding off parameter optimization due to insufficient statistics"));
		} else {
			auto [fnonlinearity, convergence] = chain.optimize_params(
			    job_.jobfile["jobconfig"].get<int>("pt_parameter_optimization_linreg_len", 2));
			job_.log(
			    fmt::format("chain {}: pt param optimization: entries={}, f nonlinearity={:.2g}, "
			                "convergence={:.2g}",
			                chain.id, chain.entries_before_optimization, fnonlinearity, convergence));

			checkpoint_write();
			chain.clear_histograms();
		}
	}
}

void runner_pt_master::react() {
	int rank_status;
	MPI_Status stat;
	MPI_Recv(&rank_status, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &stat);
	int rank = stat.MPI_SOURCE - 1;
	if(rank_status == S_BUSY) {
		int msg[1];
		MPI_Recv(msg, sizeof(msg) / sizeof(msg[0]), MPI_INT, rank + 1, 0, MPI_COMM_WORLD, &stat);
		int completed_sweeps = msg[0];

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

		if(use_param_optimization_) {
			pt_param_optimization(chain, chain_run);
		}

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
				assert(weight >= 0);
				chain_run.weight_ratios[pos] = weight;
			}
		}

		pt_global_update(chain, chain_run);
		std::fill(chain_run.weight_ratios.begin(), chain_run.weight_ratios.end(), -1);

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

		double r = rng_->random_double();

		if(r < w1 * w2) {
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

	bool use_param_optimization =
	    job_.jobfile["jobconfig"].get<bool>("pt_parameter_optimization", false);

	if(!accept_new_chain()) {
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
				sys_->pt_measure_statistics();
				pt_global_update();
				sweeps_since_last_query_++;

				timeout = negotiate_timeout();
				if(timeout != TR_CONTINUE) {
					break;
				}
			}
		}

		checkpoint_write();

		if(timeout == TR_TIMEUP) {
			send_status(S_TIMEUP);
			job_.log(fmt::format("rank {} exits: walltime up (safety interval = {} s)", rank_,
			                     sys_->safe_exit_interval()));
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
	double safe_interval = 0, max_safe_interval = 0;
	if(sys_ != nullptr) {
		safe_interval = sys_->safe_exit_interval();
	}

	MPI_Reduce(&safe_interval, &max_safe_interval, 1, MPI_DOUBLE, MPI_MAX, 0, chain_comm_);

	int result = TR_CONTINUE;
	if(chain_rank_ == 0) {
		if(MPI_Wtime() - time_last_checkpoint_ > job_.checkpoint_time) {
			result = TR_CHECKPOINT;
		}

		if(MPI_Wtime() - time_start_ > job_.walltime - max_safe_interval) {
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
	int msg[3];
	MPI_Recv(&msg, sizeof(msg) / sizeof(msg[0]), MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	task_id_ = msg[0];
	run_id_ = msg[1];
	sweeps_before_communication_ = msg[2];

	if(task_id_ < 0) {
		return false;
	}

	sweeps_per_global_update_ =
	    job_.jobfile["tasks"][job_.task_names[task_id_]].get<int>("pt_sweeps_per_global_update");

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

		int msg[1] = {sweeps_since_last_query_};
		MPI_Send(msg, sizeof(msg) / sizeof(msg[0]), MPI_INT, 0, 0, MPI_COMM_WORLD);
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
	sys_->_write_output(unique_filename);

	std::vector<evalable> evalables;
	if(job_.jobfile["jobconfig"].get<bool>("pt_parameter_optimization", false)) {
		if(rank_ == 1) {
			job_.log("Running in parameter optimization mode. No evalables are calculated.");
		}
	} else {
		sys_->register_evalables(evalables);
	}
	job_.merge_task(task_id_, evalables);
}

}
