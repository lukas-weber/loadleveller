#ifndef MCL_RUNNER_H
#define MCL_RUNNER_H

#include <vector>
#include <functional>

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
#include "one_task.h"

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

		function<abstract_mc* (string&)> my_mccreator;
		//! System
		/*! Pointer to the system class where all the magic happens.*/
		abstract_mc * sys = 0;
		
		int my_count = 0;
		int do_next = 0;

		std::string jobfile;
		std::string statusfile;
		std::string masterfile;

		std::vector<one_task> tasks;
		std::vector<string> taskfiles;
		
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
		runner();
		~runner();

		int start(const string& jobfile, double walltime, double checkpointtime, function<abstract_mc* (string &)> mccreator, int argc, char **argv);

};

#endif
