#include "runner_single.h"
#include "iodump.h"
#include "merger.h"

#include <fmt/format.h>
#include <fstream>
#include <iostream>
#include <sys/stat.h>

namespace loadl {

int runner_single_start(jobinfo job, const mc_factory &mccreator, int, char **) {
	runner_single r{std::move(job), mccreator};
	return r.start();
}

runner_single::runner_single(jobinfo job, mc_factory mccreator)
    : job_{std::move(job)}, mccreator_{std::move(mccreator)} {}

int runner_single::start() {
	time_start_ = time(nullptr);
	time_last_checkpoint_ = time_start_;

	read();
	task_id_ = get_new_task_id(task_id_);
	while(task_id_ != -1 && !time_is_up()) {
		sys_ = std::unique_ptr<mc>{mccreator_(job_.jobfile["tasks"][job_.task_names.at(task_id_)])};
		if(!sys_->_read(job_.rundir(task_id_, 1))) {
			sys_->_init();
			job_.log(fmt::format("* initialized {}", job_.rundir(task_id_, 1)));
		} else {
			job_.log(fmt::format("* read {}", job_.rundir(task_id_, 1)));
		}

		while(!tasks_[task_id_].is_done() && !time_is_up()) {
			sys_->_do_update();
			if(sys_->is_thermalized()) {
				sys_->_do_measurement();
				tasks_[task_id_].sweeps++;
			}

			if(is_checkpoint_time()) {
				checkpointing();
			}
		}

		checkpointing();
		merge_measurements();
		task_id_ = get_new_task_id(task_id_);
	}

	bool all_done = task_id_ < 0;
	return !all_done;
}

bool runner_single::is_checkpoint_time() const {
	return time(nullptr) - time_last_checkpoint_ > job_.checkpoint_time;
}

bool runner_single::time_is_up() const {
	return time(nullptr) - time_start_ > job_.runtime;
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

		size_t target_sweeps = task.get<int>("sweeps");
		size_t sweeps = 0;

		sweeps = job_.read_dump_progress(i);
		tasks_.emplace_back(target_sweeps, sweeps, 0);
	}
}

void runner_single::checkpointing() {
	time_last_checkpoint_ = time(nullptr);
	sys_->_write(job_.rundir(task_id_, 1));
	sys_->_write_finalize(job_.rundir(task_id_, 1));
	job_.log(fmt::format("* checkpointing {}", job_.rundir(task_id_, 1)));
}

void runner_single::merge_measurements() {
	std::string unique_filename = job_.taskdir(task_id_);
	sys_->write_output(unique_filename);

	std::vector<evalable> evalables;
	sys_->register_evalables(evalables);

	job_.log(fmt::format("merging {}", job_.taskdir(task_id_)));
	job_.merge_task(task_id_, evalables);
}
}
