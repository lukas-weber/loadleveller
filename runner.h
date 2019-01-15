#pragma once

#include <vector>
#include <functional>
#include <fstream>
#include <mpi.h>

#include "mc.h"
#include "definitions.h"
#include "measurements.h"
#include "dump.h"
#include "parser.h"
#include "runner_task.h"

class runner
{
	private:
		runner_task my_task;
		std::string my_rundir;
		std::string my_taskdir;

		std::function<abstract_mc* (std::string&)> my_mccreator;
		abstract_mc * sys = 0;
		
		int my_count = 0;
		int next_task_id_ = 0;

		std::string jobfile;
		std::string statusfile;
		std::string masterfile;

		std::vector<runner_task> tasks;
		std::vector<std::string> taskfiles;
		
		std::ofstream STATUS;
		
		double chktime, walltime, time_start, time_last_chkpt;		

		bool time_is_up();
		
		// master stuff
		void M_read();
		void M_wait();
		void M_send_action(int, int);
		void M_write();
		void M_end_of_run();
		void M_update(int);
		int  M_get_new_task_id();
		void M_report();
		
		// slave stuff
		void run();
		bool is_chkpt_time();
		void end_of_run();
		int recv_action();
		void what_is_next(int);
		void checkpointing();
		void merge_measurements();
		
		int N_exit;
		
	public:
		int my_rank, world_size;
		~runner();

		int start(const std::string& jobfile, double walltime, double checkpointtime, std::function<abstract_mc* (std::string &)> mccreator, int argc, char **argv);

};

