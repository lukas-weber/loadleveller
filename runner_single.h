#ifndef MCL_RUNNER_SINGLE_H
#define MCL_RUNNER_SINGLE_H

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <vector>


#ifdef NO_TASK_SHARE
#include "mc_ee.h"
#else
#include "mc.h"
#endif
#include "definitions.h"
#include "measurements.h"
#include "dump.h"
#include "parser.h"
#include "one_task.h"

class runner_single
{
	private:
		
		one_task my_task;
		std::string rundir;
		std::string taskdir;

		abstract_mc * sys;
		
		int do_next;
		
		std::string jobfile;
		std::string statusfile;
		std::string masterfile;

		std::vector<one_task> tasks;
		std::vector<string> taskfiles;
		
		std::ofstream * STATUS;
		
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
		runner_single();
		~runner_single();

		int start(const std::string& jobfile, double walltime, double checkpointtime, function<abstract_mc* (string &)> mccreator, int argc, char **argv);
};

#endif
