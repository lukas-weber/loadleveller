#include "runner.h"
#include "merger.h"
#include "iodump.h"
#include "runner_pt.h"
#include <fmt/format.h>
namespace loadl {

enum {
	MASTER = 0,
	T_STATUS = 1,
	T_ACTION = 2,
	T_NEW_JOB = 3,

	S_IDLE = 1,
	S_BUSY = 2,
	S_TIMEUP = 3,

	A_EXIT = 1,
	A_CONTINUE = 2,
	A_NEW_JOB = 3,
	A_PROCESS_DATA_NEW_JOB = 4,
};

int runner_mpi_start(jobinfo job, const mc_factory &mccreator, int argc, char **argv) {
	if(job.jobfile["jobconfig"].defined("parallel_tempering_parameter")) {
		runner_pt_start(std::move(job), mccreator, argc, argv);
		return 0;
	}

	MPI_Init(&argc, &argv);

	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	if(rank == 0) {
		runner_master r{std::move(job)};
		r.start();
	} else {
		runner_slave r{std::move(job), mccreator};
		r.start();
	}

	MPI_Finalize();

	return 0;
}

runner_master::runner_master(jobinfo job) : job_{std::move(job)} {}

void runner_master::start() {
	MPI_Comm_size(MPI_COMM_WORLD, &num_active_ranks_);

	job_.log(fmt::format("Starting job '{}'", job_.jobname));
	read();

	while(num_active_ranks_ > 1) {
		react();
	}
}

int runner_master::get_new_task_id(int old_id) {
	int ntasks = tasks_.size();
	int i;
	for(i = 1; i <= ntasks; i++) {
		if(!tasks_[(old_id + i) % ntasks].is_done())
			return (old_id + i) % ntasks;
	}

	// everything done!
	return -1;
}

void runner_master::react() {
	int node_status;
	MPI_Status stat;
	MPI_Recv(&node_status, 1, MPI_INT, MPI_ANY_SOURCE, T_STATUS, MPI_COMM_WORLD, &stat);
	int node = stat.MPI_SOURCE;
	if(node_status == S_IDLE) {
		current_task_id_ = get_new_task_id(current_task_id_);

		if(current_task_id_ < 0) {
			send_action(A_EXIT, node);
			num_active_ranks_--;
		} else {
			send_action(A_NEW_JOB, node);
			tasks_[current_task_id_].scheduled_runs++;
			int msg[3] = {current_task_id_, tasks_[current_task_id_].scheduled_runs,
			              tasks_[current_task_id_].target_sweeps +
			                  tasks_[current_task_id_].target_thermalization -
			                  tasks_[current_task_id_].sweeps};
			MPI_Send(&msg, sizeof(msg) / sizeof(msg[0]), MPI_INT, node, T_NEW_JOB, MPI_COMM_WORLD);
		}
	} else if(node_status == S_BUSY) {
		int msg[2];
		MPI_Recv(msg, sizeof(msg) / sizeof(msg[0]), MPI_INT, node, T_STATUS, MPI_COMM_WORLD, &stat);
		int task_id = msg[0];
		int completed_sweeps = msg[1];

		tasks_[task_id].sweeps += completed_sweeps;
		if(tasks_[task_id].is_done()) {
			tasks_[task_id].scheduled_runs--;

			if(tasks_[task_id].scheduled_runs > 0) {
				job_.log(fmt::format("{} has enough sweeps. Waiting for {} busy ranks.",
				                     job_.task_names[task_id], tasks_[task_id].scheduled_runs));
				send_action(A_NEW_JOB, node);
			} else {
				job_.log(fmt::format("{} is done. Merging.", job_.task_names[task_id]));

				send_action(A_PROCESS_DATA_NEW_JOB, node);
			}
		} else {
			send_action(A_CONTINUE, node);
		}
	} else { // S_TIMEUP
		num_active_ranks_--;
	}
}

void runner_master::send_action(int action, int destination) {
	MPI_Send(&action, 1, MPI_INT, destination, T_ACTION, MPI_COMM_WORLD);
}

void runner_master::read() {
	for(size_t i = 0; i < job_.task_names.size(); i++) {
		auto task = job_.jobfile["tasks"][job_.task_names[i]];

		int target_sweeps = task.get<int>("sweeps");
		int target_thermalization = task.get<int>("thermalization");
		int sweeps = job_.read_dump_progress(i);
		int scheduled_runs = 0;

		tasks_.emplace_back(target_sweeps, target_thermalization, sweeps, scheduled_runs);
	}
}

runner_slave::runner_slave(jobinfo job, mc_factory mccreator)
    : job_{std::move(job)}, mccreator_{std::move(mccreator)} {}

void runner_slave::start() {
	MPI_Comm_rank(MPI_COMM_WORLD, &rank_);
	time_start_ = MPI_Wtime();
	time_last_checkpoint_ = time_start_;

	int action = what_is_next(S_IDLE);
	while(action != A_EXIT) {
		if(action == A_NEW_JOB) {
			sys_ =
			    std::unique_ptr<mc>{mccreator_(job_.jobfile["tasks"][job_.task_names[task_id_]])};
			if(!sys_->_read(job_.rundir(task_id_, run_id_))) {
				sys_->_init();
				job_.log(fmt::format("* initialized {}", job_.rundir(task_id_, run_id_)));
				checkpoint_write();
			} else {
				job_.log(fmt::format("* read {}", job_.rundir(task_id_, run_id_)));
			}
		} else {
			if(!sys_) {
				throw std::runtime_error(
				    "slave got A_CONTINUE even though there is no job to be continued");
			}
		}

		while(sweeps_since_last_query_ < sweeps_before_communication_) {
			sys_->_do_update();
			sweeps_since_last_query_++;

			if(sys_->is_thermalized()) {
				sys_->_do_measurement();
			}

			if(is_checkpoint_time() || time_is_up()) {
				break;
			}
		}
		checkpoint_write();

		if(time_is_up()) {
			what_is_next(S_TIMEUP);
			job_.log(fmt::format("rank {} exits: walltime up (safety interval = {} s)", rank_,
			                     sys_->safe_exit_interval()));
			break;
		}

		action = what_is_next(S_BUSY);
	}

	if(action == A_EXIT) {
		job_.log(fmt::format("rank {} exits: out of work", rank_));
	}
}

bool runner_slave::is_checkpoint_time() {
	return MPI_Wtime() - time_last_checkpoint_ > job_.checkpoint_time;
}

bool runner_slave::time_is_up() {
	double safe_interval = 0;
	if(sys_ != nullptr) {
		safe_interval = sys_->safe_exit_interval();
	}
	return MPI_Wtime() - time_start_ > job_.walltime - safe_interval;
}

int runner_slave::what_is_next(int status) {
	MPI_Send(&status, 1, MPI_INT, MASTER, T_STATUS, MPI_COMM_WORLD);
	if(status == S_TIMEUP) {
		return 0;
	} else if(status == S_IDLE) {
		int new_action = recv_action();
		if(new_action == A_EXIT) {
			return A_EXIT;
		}
		MPI_Status stat;
		int msg[3];
		MPI_Recv(&msg, sizeof(msg) / sizeof(msg[0]), MPI_INT, 0, T_NEW_JOB, MPI_COMM_WORLD, &stat);
		task_id_ = msg[0];
		run_id_ = msg[1];
		sweeps_before_communication_ = msg[2];

		return A_NEW_JOB;
	}

	int msg[2] = {task_id_, sweeps_since_last_query_};
	MPI_Send(msg, sizeof(msg) / sizeof(msg[0]), MPI_INT, 0, T_STATUS, MPI_COMM_WORLD);
	sweeps_since_last_query_ = 0;
	int new_action = recv_action();
	if(new_action == A_PROCESS_DATA_NEW_JOB) {
		merge_measurements();
		return what_is_next(S_IDLE);
	}
	if(new_action == A_NEW_JOB) {
		return what_is_next(S_IDLE);
	}
	if(new_action == A_EXIT) {
		return A_EXIT;
	}

	return A_CONTINUE;
}

int runner_slave::recv_action() {
	MPI_Status stat;
	int new_action;
	MPI_Recv(&new_action, 1, MPI_INT, MASTER, T_ACTION, MPI_COMM_WORLD, &stat);
	return new_action;
}

void runner_slave::checkpoint_write() {
	time_last_checkpoint_ = MPI_Wtime();
	sys_->_write(job_.rundir(task_id_, run_id_));
	job_.log(fmt::format("* rank {}: checkpoint {}", rank_, job_.rundir(task_id_, run_id_)));
}

void runner_slave::merge_measurements() {
	std::string unique_filename = job_.taskdir(task_id_);
	sys_->_write_output(unique_filename);

	std::vector<evalable> evalables;
	sys_->register_evalables(evalables);
	job_.merge_task(task_id_, evalables);
}
}
