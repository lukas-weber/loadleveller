#include "runner_single.h"
#include "dump.h"
#include "merger.h"

#include <fmt/format.h>
#include <fstream>
#include <iostream>
#include <sys/stat.h>

namespace loadl {

int runner_single_start(jobinfo job, const mc_factory &mccreator, int, char **) {
	runner_single r{std::move(job), mccreator};
	r.start();
	return 0;
}

runner_single::runner_single(jobinfo job, mc_factory mccreator)
    : job_{std::move(job)}, mccreator_{std::move(mccreator)} {}

int runner_single::start() {
	time_start_ = time(nullptr);
	time_last_checkpoint_ = time_start_;

	read();
	task_id_ = get_new_task_id(task_id_);
	while(task_id_ != -1 && !time_is_up()) {
		sys = std::unique_ptr<mc>{mccreator_(job_.jobfile_name, job_.task_names.at(task_id_))};
		if(sys->_read(job_.rundir(task_id_, 1))) {
			job_.status << 0 << " : L " << job_.rundir(task_id_, 1) << "\n";
		} else {
			job_.status << 0 << " : I " << job_.rundir(task_id_, 1) << "\n";
			sys->_init();
		}

		while(!tasks_[task_id_].is_done() && !time_is_up()) {
			sys->_do_update();
			tasks_[task_id_].sweeps++;
			if(sys->is_thermalized()) {
				sys->_do_measurement();
			}

			if(is_checkpoint_time()) {
				checkpointing();
			}
		}

		checkpointing();
		merge_measurements();
		task_id_ = get_new_task_id(task_id_);
	}
	end_of_run();

	return 0;
}

bool runner_single::is_checkpoint_time() const {
	return time(nullptr) - time_last_checkpoint_ > job_.checkpoint_time;
}

bool runner_single::time_is_up() const {
	return time(nullptr) - time_start_ > job_.walltime;
}

int runner_single::get_new_task_id(int old_id) {
	int ntasks = tasks_.size();
	int i;
	for(i = 1; i <= ntasks; i++) {
		if(!tasks_[(old_id + i) % ntasks].is_done()) {
			return (old_id + i) % ntasks;
		}
	}

	// everything done!
	return -1;
}

void runner_single::read() {
	for(size_t i = 0; i < job_.task_names.size(); i++) {
		auto task = job_.jobfile["tasks"][job_.task_names[i]];

		int target_sweeps = task.get<int>("sweeps");
		int target_thermalization = task.get<int>("thermalization");
		int sweeps = 0;

		try {
			iodump dump = iodump::open_readonly(job_.rundir(i, 1) + ".dump.h5");
			dump.get_root().read("sweeps", sweeps);
		} catch(iodump_exception &e) {
		}

		tasks_.emplace_back(target_sweeps, target_thermalization, sweeps, 0);
	}
	report();
}

void runner_single::end_of_run() {
	report();
	job_.status << 0 << " : finalized"
	            << "\n";
}

void runner_single::report() {
	for(size_t i = 0; i < tasks_.size(); i++) {
		job_.status << fmt::format(
		    "{:4d} {:3.0f}%\n", i,
		    tasks_[i].sweeps /
		        static_cast<double>(tasks_[i].target_sweeps + tasks_[i].target_thermalization) *
		        100);
	}
}

void runner_single::checkpointing() {
	time_last_checkpoint_ = time(nullptr);
	sys->_write(job_.rundir(task_id_, 1));
	job_.status << "0 : C " << job_.rundir(task_id_, 1) << "\n";
}

void runner_single::merge_measurements() {
	std::string unique_filename = job_.taskdir(task_id_);
	sys->_write_output(unique_filename);

	std::vector<evalable> evalables;
	sys->register_evalables(evalables);

	job_.merge_task(task_id_, evalables);

	job_.status << "0 : M " << job_.taskdir(task_id_) << "\n";
}
}
