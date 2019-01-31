#pragma once

#include <vector>
#include <functional>
#include <fstream>
#include <mpi.h>

#include "mc.h"
#include "measurements.h"
#include "dump.h"
#include "parser.h"
#include "runner_task.h"

class runner {
private:
	enum {
		MASTER = 0,
		T_STATUS = 1,
		T_ACTION = 2,
		T_NEW_JOB = 3,
		T_PARTNER = 4,
		T_WEIGHT = 5,
		T_LABEL = 6,
		T_POS = 7,

		S_IDLE = 1,
		S_BUSY = 2,
		S_FINISHED = 3,
		S_GLOBAL_UPDATE = 4,
		S_CHECKPOINT = 5,

		A_EXIT = 1,
		A_CONTINUE = 2,
		A_NEW_JOB = 3,
		A_CONTINUE_JOB = 4,
		A_PROCESS_DATA_NEW_JOB = 5,
		A_GLOBAL_UPDATE	= 6,
		A_CHECKPOINT = 7
	};
	
	mc_factory mccreator_;
	std::unique_ptr<abstract_mc> sys;
	int my_rank, world_size;


	std::string jobfile_name_;
	std::unique_ptr<parser> jobfile_;

	std::vector<runner_task> tasks_;
	std::vector<std::string> task_names_;

	std::ofstream STATUS;
	
	double chktime_;
	double walltime_;
	double time_start_;
	double time_last_chkpt_;

	int sweeps_before_communication_;
	
	bool time_is_up();
	std::string rundir(int task_id, int run_id) const;
	std::string taskdir(int task_id) const;
	static std::vector<std::string> list_run_files(const std::string& taskdir, const std::string& file_ending);
	
	// master stuff
	int current_task_id_{-1};
	int N_exit{0}; // number of ranks that have finished

	void M_wait();
	void M_read();
	int M_read_dump_progress(int task_id);
	void M_send_action(int action, int destination);
	void M_end_of_run();
	void M_update(int action);
	int  M_get_new_task_id(int old_id);
	void M_report();
	
	// slave stuff
	int task_id_{0};
	int run_id_{0};
	void run();
	bool is_chkpt_time();
	void end_of_run();
	int recv_action();
	void what_is_next(int);
	void checkpointing();
	void merge_measurements();
	

public:
	int start(const std::string& jobfile, const mc_factory& mccreator, int argc, char **argv);

};

