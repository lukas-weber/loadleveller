#include "runner_single.h"
#include "dump.h"
#include "merger.h"

#include <fmt/format.h>
#include <fmt/printf.h>

runner_single::~runner_single() {
	delete sys;
}


int runner_single :: start(const std::string& jobfile, double walltime, double checkpointtime, std::function<abstract_mc* (std::string &)> mccreator, int argc, char **argv)
{
	this->jobfile = jobfile;
	this->walltime = walltime;
	chktime = checkpointtime;
	
	parser parsedfile(jobfile);

	taskfiles = parsedfile.return_vector<std::string>("@taskfiles");
	time_start = time(NULL);
	time_last_chkpt = time_start;
	//statusfile = parsedfile.value_or_default<std::string>("statusfile", jobfile + ".status");
	masterfile = parsedfile.value_or_default<std::string>("masterfile", jobfile + ".master");


	read();
	int task_id = get_new_task_id();
	while (task_id != -1) {
		std::string taskfile = taskfiles[task_id];
		parser cfg(taskfile);
		taskdir = cfg.value_or_default("taskdir", fmt::format("task{:04d}",task_id + 1));
		rundir = "/run1";
		sys = mccreator(taskfile);
		if(sys->_read(rundir)) {
			STATUS << 0 << " : L " << rundir << "\n";
		} else {
			STATUS << 0 << " : I " << rundir << "\n";
			sys->_init();
			checkpointing();
		}
		while (tasks[task_id].mes_done < tasks[task_id].n_steps) {
			sys->_do_update();
			++tasks[task_id].steps_done;
			if (sys->is_thermalized()) {
				sys->do_measurement();
				++tasks[task_id].mes_done;
			}
			if (is_chkpt_time()) {
				checkpointing();
				write();
				if (time_is_up()) {
					end_of_run();
				}
			}

		}
		checkpointing();
		write();
		merge_measurements();
		delete sys;
		tasks[task_id].is_done = 1;
		task_id = get_new_task_id();
	}
	write();
	end_of_run();

	return 0;
}

bool runner_single::is_chkpt_time() {
	if ((time(NULL) - time_last_chkpt) > chktime) {
		time_last_chkpt = time(NULL);
		return true;
	}
	return false;
}

bool runner_single::time_is_up() {
	return ((time(NULL) - time_start) > walltime);
}

int runner_single::get_new_task_id() {
	next_task_id_ = (next_task_id_ + 1) % (int) tasks.size();
	int count = 0;
	while (tasks[next_task_id_].is_done == 1 && count <= (int) (tasks.size())) {
		++count;
		next_task_id_ = (next_task_id_ + 1) % (int) tasks.size();
	}
	return (count >= (int) tasks.size()) ? -1 : next_task_id_;
}

void runner_single::write() {
	iodump d = iodump::create(masterfile);
	d.write("next_task_id", next_task_id_-1);
	d.change_group("tasks");
	for(const auto& task : tasks) {
		d.change_group(fmt::format("{:04d}", task.task_id));
		task.checkpoint_write(d);
		d.change_group("..");
	}
	d.change_group("..");
}

void runner_single::read() {
	try {
		iodump d = iodump::open_readonly(masterfile);

		d.read("next_task_id", next_task_id_);
		d.change_group("tasks");
		for(const auto& task_name : d.list()) {
			d.change_group(task_name);
			tasks.emplace_back();
			tasks.back().checkpoint_read(d);
			d.change_group("..");
		}		
	} catch(iodump_exception e) {
		fmt::printf("runner_single::read: failed to read master file: {}. Discarding progress information.", e.what());
		next_task_id_= -1;
	}

	for (uint i = 0; i < taskfiles.size(); i++) {
		parser tp(taskfiles[i]);
		runner_task new_task;
		if (tp.defined("SWEEPS"))
			new_task.n_steps = tp.return_value_of<int>("SWEEPS");
		if (tp.defined("n_steps"))
			new_task.n_steps = tp.return_value_of<int>("n_steps");
		if (i < tasks.size()) {
			if (new_task.n_steps > tasks[i].n_steps)
				tasks[i].is_done = 0;
			if (new_task.n_steps != tasks[i].n_steps)
				tasks[i].n_steps = new_task.n_steps;
		} else {
			new_task.task_id = i;
			new_task.is_done = 0;
			new_task.steps_done = 0;
			new_task.mes_done = 0;
			tasks.push_back(new_task);
		}
	}
	report();
}

void runner_single::end_of_run() {
	bool need_restart = false;
	for (uint i = 0; i < tasks.size(); i++)
		if (tasks[i].is_done == 0)
			need_restart = true;
	if (need_restart) {
		std::string rfilename = jobfile + ".restart";
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
	for (uint i = 0; i < tasks.size(); i++) {
		STATUS << tasks[i].task_id << "\t"
				<< int(tasks[i].mes_done / (double) (tasks[i].n_steps) * 100) << "%\t"
				<< tasks[i].steps_done << "\t" << tasks[i].mes_done << "\n";
	}
	STATUS << "\n";
}

void runner_single::checkpointing() {
	sys->_write(rundir);
	STATUS << "0 : C " << rundir << "\n";
}

void runner_single::merge_measurements() {
	std::string mf = taskdir + ".out";
	sys->_write_output(mf);
	STATUS << 0 << " : M " << taskdir << "\n";
}

