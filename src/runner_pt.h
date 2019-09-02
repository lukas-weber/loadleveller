#pragma once
#include "runner.h"
#include <map>
#include <vector>

namespace loadl {

struct pt_chain {
	int id{};
	std::vector<int> task_ids;
	std::vector<double> params;

	int sweeps{-1};
	int target_sweeps{-1};
	int target_thermalization{-1};
	int scheduled_runs{};

	// parameter optimization
	int entries_before_optimization{0};

	std::vector<double> rejection_rates;
	std::vector<int> rejection_rate_entries{0,0};

	bool is_done();
	void checkpoint_read(const iodump::group &g);
	void checkpoint_write(const iodump::group &g);

	void clear_histograms();
	std::tuple<double, double> optimize_params();
};

struct pt_chain_run {
private:
	pt_chain_run() = default;

public:
	int id{};
	int run_id{};
	bool swap_odd{};

	std::vector<int> rank_to_pos;
	std::vector<int> switch_partners;
	std::vector<double> weight_ratios;

	pt_chain_run(const pt_chain &chain, int run_id);
	static pt_chain_run checkpoint_read(const iodump::group &g);
	void checkpoint_write(const iodump::group &g);
};

int runner_pt_start(jobinfo job, const mc_factory &mccreator, int argc, char **argv);

class runner_pt_master {
private:
	jobinfo job_;
	int num_active_ranks_{0};

	double time_last_checkpoint_{0};

	// parameter optimization
	struct {
		bool enabled{};
		int nsamples_initial{};
		double nsamples_growth{};
	} po_config_;

	std::vector<pt_chain> pt_chains_;
	std::vector<pt_chain_run> pt_chain_runs_;
	int chain_len_;
	std::unique_ptr<random_number_generator> rng_;

	std::map<int, int> rank_to_chain_run_;
	int current_chain_id_{-1};

	measurements pt_meas_;

	void construct_pt_chains();
	void checkpoint_write();
	void checkpoint_read();
	void write_params_json();
	void write_param_optimization_stats();

	int schedule_chain_run();
	void pt_global_update(pt_chain &chain, pt_chain_run &chain_run);
	void pt_param_optimization(pt_chain &chain);

	void react();
	void send_action(int action, int destination);
	int assign_new_chain(int rank_section);

public:
	runner_pt_master(jobinfo job);
	void start();
};

class runner_pt_slave {
private:
	jobinfo job_;

	mc_factory mccreator_;
	std::unique_ptr<mc> sys_;

	MPI_Comm chain_comm_;
	int chain_rank_{};

	double time_last_checkpoint_{0};
	double time_start_{0};

	int rank_{};
	int sweeps_since_last_query_{};
	int sweeps_before_communication_{};
	int sweeps_per_global_update_{};
	int task_id_{-1};
	int run_id_{-1};

	double current_param_{};

	void pt_global_update();

	int negotiate_timeout();

	void send_status(int status);
	int recv_action();
	void checkpoint_write();
	void merge_measurements();
	bool accept_new_chain();
	int what_is_next(int status);

public:
	runner_pt_slave(jobinfo job, mc_factory mccreator);
	void start();
};
}
