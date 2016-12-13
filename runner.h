#ifndef MCL_RUNNER_H
#define MCL_RUNNER_H

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <vector>

#include <mpi.h>

#ifdef NO_TASK_SHARE
#include "mc_ee.h"
#else
#include "mc.h"
#endif

#include "definitions.h"
#include "measurements.h"
#include "dump.h"
#include "parser.h"

//! One_task
/*! Carries the information for one task and is transferred by mpi */
struct one_task
{
	int task_id;
	int is_done;
	int n_steps;
	int steps_done;
	int mes_done;
	int run_counter; 
};

//! Runner
/*! The main interface for inter-process communications and job management. The master and slave members are included here. */
class runner
{
	private:
		
		//! My Task
		/*! The current task of the node. */
		one_task my_task;
		//! Rundir
		/*! The current running dir. */
		std::string my_rundir;
		//! Taskdir
		/*! The current taskdir. */
		std::string my_taskdir;

		//! System
		/*! Pointer to the system class where all the magic happens.*/
		mc * sys;
		
		int my_count;		
		int do_next;
		
		std::string jobfile;
		std::string statusfile;
		std::string masterfile;

		std::vector<one_task> tasks;
		std::vector<string> taskfiles;
		
		std::ofstream * STATUS;
		
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
		runner(int argc, char *argv[]);
		~runner();

		void start();

};

#endif
