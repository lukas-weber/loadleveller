#pragma once

#include <vector>
#include <functional>
#include <ostream>
#include <mpi.h>

#include "mc.h"
#include "measurements.h"
#include "dump.h"
#include "parser.h"
#include "runner_task.h"

class runner_mpi {
public:
	static int start(const std::string& jobfile_name, const mc_factory& mccreator, int argc, char **argv);
};
	
struct jobinfo {
	std::string jobfile_name;
	parser jobfile;

	std::vector<std::string> task_names;
	
	double checkpoint_time;
	double walltime;
	int sweeps_before_communication;
	
	std::ostream& status{std::cout};
	
	jobinfo(const std::string &jobfile_name);

	std::string rundir(int task_id, int run_id) const;
	std::string taskdir(int task_id) const;
	
	static std::vector<std::string> list_run_files(const std::string& taskdir, const std::string& file_ending);
};

class runner_master {
private:
	jobinfo job_;

	int num_active_ranks_;
	int time_start_;

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
	runner_master(jobinfo&& job);
	void start();
};

class runner_slave {
private:
	jobinfo job_;
	
	mc_factory mccreator_;
	std::unique_ptr<abstract_mc> sys_;

	double time_last_checkpoint_;
	double time_start_;

	int rank_;
	int sweeps_since_last_query_{0};
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
	runner_slave(jobinfo&& job, const mc_factory& mccreator);
	void start();
};

