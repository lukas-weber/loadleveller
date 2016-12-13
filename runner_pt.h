#ifndef MCL_RUNNER_H
#define MCL_RUNNER_H

#include <mpi.h>

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <vector>

#include "mc_pt.h"
#include "definitions.h"
#include "measurements.h"
#include "dump.h"
#include "parser.h"

struct one_task
{
	int task_id;
	int n_steps;
	int is_done;
	int run_counter;
	int steps_done;  //for master-slave communication
	int mes_done;    //for master-slave communication
	int pt_pos;      //for master-slave communication 
};

class runner_pt
{
	private:
		
		one_task my_task;
		std::string my_taskdir;
		std::string my_rundir;
	
		mc_pt * sys;
		
		std::string jobfile;
		std::string statusfile;
		std::string masterfile;
		std::string acceptfile;
		std::string positionsfile;
		
		std::vector<one_task> tasks;
		std::vector<string> taskfiles;
		
		std::vector<int> pt_node_task;
		std::vector<int> pt_node_order;
		std::vector<int> pt_node_mes_done;
		std::vector<int> pt_node_steps_done;
#ifdef MCL_HISTOGRAM
		std::vector<int> histogram_up,
			histogram_down;
#endif
#ifdef MCL_POS
		std::stringstream pt_position_order;
#endif
		std::vector<int> pt_accepted;
		int pt_moves;

		randomnumbergenerator * pt_rng;

		std::ofstream * STATUS;

		bool * pt_node_waits_for_global_update;
		int pt_N_global_update_waiter;
		
		double chktime, walltime, time_start, time_last_chkpt;

		bool time_is_up();
		
		// master stuff
		void M_read();
		void M_wait();
		void M_send_action(int, int);
		void M_write();
		void M_end_of_run();
		void M_update(int);
		void M_report();
		void M_report_acc_ratio();
#ifdef MCL_POS
		void M_report_pt_positions();
#endif
		void M_global_update();
		
		// slave stuff
		void run();
		bool is_chkpt_time();
		void end_of_run();
		int recv_action();
		void what_is_next(int);
		void global_update();
		void checkpointing();
		void merge_measurements();
		
		int N_exit;
		
		
	public:
		int my_rank, world_size;
		runner_pt(int argc, char *argv[]);
		~runner_pt();

		void start();

};

#endif
