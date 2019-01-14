#pragma once

#include "mc.h"
#include "runner_task.h"
#include <iostream>
#include <functional>

class runner_single {
private:
	
	runner_task my_task;
	std::string rundir;
	std::string taskdir;

	abstract_mc *sys;
	
	int next_task_id_;
	
	std::string jobfile;
	std::string statusfile;
	std::string masterfile;

	std::vector<runner_task> tasks;
	std::vector<std::string> taskfiles;
	
	std::ostream& STATUS = std::cout;
	
	double chktime, walltime, time_start, time_last_chkpt;		
	
	void read();
	bool time_is_up();
	void write();
	void end_of_run();
	int  get_new_task_id();
	void report();
	
	bool is_chkpt_time();
	void checkpointing();
	void merge_measurements();
	
	
public:
	~runner_single();

	int start(const std::string& jobfile, double walltime, double checkpointtime, std::function<abstract_mc* (std::string &)> mccreator, int argc, char **argv);
};

