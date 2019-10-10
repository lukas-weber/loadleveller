#pragma once

#include <functional>
#include <mpi.h>
#include <ostream>
#include <vector>

#include "jobinfo.h"
#include "mc.h"
#include "measurements.h"
#include "parser.h"
#include "runner_task.h"

namespace loadl {

int runner_mpi_start(jobinfo job, const mc_factory &mccreator, int argc, char **argv);

class runner_master {
private:
	jobinfo job_;

	int num_active_ranks_{0};

	std::vector<runner_task> tasks_;
	int current_task_id_{-1};

	void read();
	int get_new_task_id(int old_id);

	void react();
	void send_action(int action, int destination);

public:
	runner_master(jobinfo job);
	void start();
};

class runner_slave {
private:
	jobinfo job_;

	mc_factory mccreator_;
	std::unique_ptr<mc> sys_;

	double time_last_checkpoint_{0};
	double time_start_{0};

	int rank_{0};
	size_t sweeps_since_last_query_{0};
	size_t sweeps_before_communication_{0};
	int task_id_{-1};
	int run_id_{-1};

	bool is_checkpoint_time();
	bool time_is_up();
	void end_of_run();
	int recv_action();
	int what_is_next(int);
	void checkpoint_write();
	void merge_measurements();

public:
	runner_slave(jobinfo job, mc_factory mccreator);
	void start();
};
}
