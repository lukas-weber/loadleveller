#include "runner_single.h"
#include "dump.h"
#include "merger.h"

#include <fstream>
#include <fmt/format.h>
#include <iostream>
#include <sys/stat.h>

// these are crafted to be compatible with an mpi run.
std::string runner_single::taskdir(int task_id) const {
	return fmt::format("{}.{}", jobfile_name_, task_names_.at(task_id));
}
std::string runner_single::rundir(int task_id) const {
	return taskdir(task_id)+"/run0001";
}

int runner_single::start(const std::string& jobfile_name, const mc_factory& mccreator, int argc, char **argv)
{
	jobfile_name_ = jobfile_name;
	jobfile_ = std::unique_ptr<parser>{new parser{jobfile_name}};

	parser jobconfig{jobfile_->get<std::string>("jobconfig")};

	for(auto node : (*jobfile_)["tasks"]) {
		std::string task_name = node.first;
		std::cerr << task_name << '\n';
		task_names_.push_back(task_name);
	}

	for(size_t i = 0; i < task_names_.size(); i++) {
		int rc = mkdir(taskdir(i).c_str(), 0755);
		if(rc && errno != EEXIST) {
			throw std::runtime_error{fmt::format("creation of output directory '{}' failed: {}", taskdir(i), strerror(errno))};
		}
	}

	walltime_ = jobconfig.get<double>("mc_walltime");
	chktime_ = jobconfig.get<double>("mc_checkpoint_time");
	
	time_start_ = time(NULL);
	time_last_chkpt_ = time_start_;

	read();
	task_id_ = get_new_task_id(task_id_);
	while(task_id_ != -1) {
		sys = std::unique_ptr<abstract_mc>{mccreator(jobfile_name, task_names_[task_id_])};
		if(sys->_read(rundir(task_id_))) {
			STATUS << 0 << " : L " << rundir(task_id_) << "\n";
		} else {
			STATUS << 0 << " : I " << rundir(task_id_) << "\n";
			sys->_init();
			checkpointing();
		}
		
		while (!tasks_[task_id_].is_done()) {
			sys->_do_update();
			tasks_[task_id_].sweeps++;
			if (sys->is_thermalized()) {
				sys->do_measurement();
			}
			if (is_chkpt_time()) {
				checkpointing();
				if (time_is_up()) {
					end_of_run();
				}
			}

		}

		checkpointing();
		merge_measurements();
		task_id_ = get_new_task_id(task_id_);
	}
	end_of_run();

	return 0;
}

bool runner_single::is_chkpt_time() {
	if ((time(NULL) - time_last_chkpt_) > chktime_) {
		time_last_chkpt_ = time(NULL);
		return true;
	}
	return false;
}

bool runner_single::time_is_up() {
	return ((time(NULL) - time_start_) > walltime_);
}

int runner_single::get_new_task_id(int old_id) {
	int ntasks = tasks_.size();
	int i;
	for(i = 1; i <= ntasks; i++) {
		if(!tasks_[(old_id+i)%ntasks].is_done())
			return (old_id+i)%ntasks;
	}

	// everything done!
	return -1;
}

void runner_single::read() {
	for(size_t i = 0; i < task_names_.size(); i++) {
		auto task = (*jobfile_)["tasks"][task_names_[i]];

		int target_sweeps = task.get<int>("sweeps");
		int target_thermalization = task.get<int>("thermalization");
		int sweeps = 0;

		try {
			iodump dump = iodump::open_readonly(rundir(i)+".dump.h5");
			dump.get_root().read("sweeps", sweeps);
		} catch(iodump_exception& e) {
		}

		tasks_.emplace_back(target_sweeps, target_thermalization, sweeps, 0);
	}
	report();
}

void runner_single::end_of_run() {
	bool need_restart = false;
	for (size_t i = 0; i < tasks_.size(); i++) {
		if(!tasks_[i].is_done()) {
			need_restart = true;
			break;
		}
	}
	if (need_restart) {
		std::string rfilename = jobfile_name_ + ".restart";
		std::ofstream rfile(rfilename.c_str());
		rfile << "restart me\n";
		rfile.close();
		STATUS << 0 << " : Restart needed" << "\n";
	}
	report();
	STATUS << 0 << " : finalized" << "\n";
	exit(0);
}

void runner_single::report() {
	for(size_t i = 0; i < tasks_.size(); i ++) {
		STATUS << fmt::format("{:4d} {:3.0f}%\n", i,
				      tasks_[i].sweeps/(double)(tasks_[i].target_sweeps + tasks_[i].target_thermalization)*100);
	}
}

void runner_single::checkpointing() {
	sys->_write(rundir(task_id_));
	STATUS << "0 : C " << rundir(task_id_) << "\n";
}

void runner_single::merge_measurements() {
	std::string mf = taskdir(task_id_) + ".out";
	sys->_write_output(mf);
	STATUS << 0 << " : M " << taskdir(task_id_) << "\n";
}

