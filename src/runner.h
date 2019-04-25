#pragma once

#include <functional>
#include <mpi.h>
#include <ostream>
#include <vector>

#include "dump.h"
#include "mc.h"
#include "measurements.h"
#include "parser.h"
#include "runner_task.h"

namespace loadl {

struct jobinfo {
	std::string jobfile_name;
	parser jobfile;

	std::vector<std::string> task_names;

	double checkpoint_time;
	double walltime;

	std::ostream &status{std::cout};

	jobinfo(const std::string &jobfile_name);

	std::string rundir(int task_id, int run_id) const;
	std::string taskdir(int task_id) const;

	static std::vector<std::string> list_run_files(const std::string &taskdir,
	                                               const std::string &file_ending);
	void merge_task(int task_id, const std::vector<evalable> &evalables);
	void concatenate_results();
};

int runner_mpi_start(jobinfo job, const mc_factory &mccreator, int argc, char **argv);

class runner_master {
private:
	jobinfo job_;

	int num_active_ranks_{0};
	int time_start_{0};

	std::vector<runner_task> tasks_;
	int current_task_id_{-1};

	bool time_is_up() const;

	void read();
	int read_dump_progress(int task_id);
	int get_new_task_id(int old_id);

	void react();
	void send_action(int action, int destination);

	void end_of_run();
	void report();

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
	int sweeps_since_last_query_{0};
	int sweeps_before_communication_{0};
	int task_id_{-1};
	int run_id_{-1};

	bool is_checkpoint_time();
	bool time_is_up();
	void end_of_run();
	int recv_action();
	int what_is_next(int);
	void checkpointing();
	void merge_measurements();

public:
	runner_slave(jobinfo job, mc_factory mccreator);
	void start();
};
}
