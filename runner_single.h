#pragma once

#include "mc.h"
#include "runner_task.h"
#include <ostream>
#include <functional>

class runner_single {
private:
	std::unique_ptr<abstract_mc> sys;
	
	int task_id_;
	
	std::string jobfile_name_;
	std::unique_ptr<parser> jobfile_;

	std::vector<runner_task> tasks_;
	std::vector<std::string> task_names_;
	
	std::ostream& STATUS = std::cout;
	
	double chktime_;
	double walltime_;
	double time_start_;
	double time_last_chkpt_;

	std::string taskdir(int task_id) const;
	std::string rundir(int task_id) const;
	
	void read();
	bool time_is_up();
	void end_of_run();
	int  get_new_task_id(int old_id);
	void report();
	
	bool is_chkpt_time();
	void checkpointing();
	void merge_measurements();
public:
	int start(const std::string& jobfile, const mc_factory& mccreator, int argc, char **argv);
};

