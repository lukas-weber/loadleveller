#include "runner_single.h"
#include "merge.h"

runner_single::runner_single() {
}

runner_single::~runner_single() {
	(*STATUS).close();
	delete STATUS;
	delete sys;
}


int runner_single :: start(const std::string& jobfile, double walltime, double checkpointtime, function<abstract_mc* (string &)> mccreator, int argc, char **argv)
{
	this->jobfile = jobfile;
	this->walltime = walltime;
	chktime = checkpointtime;
	
	parser parsedfile(jobfile);

	taskfiles = parsedfile.return_vector<string>("@taskfiles");
	time_start = time(NULL);
	time_last_chkpt = time_start;
	statusfile = parsedfile.value_or_default<string>("statusfile", jobfile + ".status");
	masterfile = parsedfile.value_or_default<string>("masterfile", jobfile + ".master");
	STATUS = new std::ofstream(statusfile.c_str(), std::ios::out | std::ios::app);


	read();
	int task_id = get_new_task_id();
	while (task_id != -1) {
		std::string taskfile = taskfiles[task_id];
		stringstream tb;
		tb << "task" << task_id + 1;
		parser cfg(taskfile);
		taskdir = cfg.value_or_default("taskdir", tb.str());
		stringstream rb;
		rb << taskdir << "/run" << 1 << ".";
		rundir = rb.str();
		sys = mccreator(taskfile);
		if (sys->measure.read(rundir) && sys->_read(rundir)) {
			(*STATUS) << 0 << " : L " << rundir << "\n";
			cerr << 0 << " : L " << rundir << "\n";
		} else {
			(*STATUS) << 0 << " : I " << rundir << "\n";
			cerr << 0 << " : I " << rundir << "\n";
			sys->_init();
			checkpointing();
		}
#ifdef NO_TASK_SHARE
		while (sys->work_done()<0.5) {
#else
		while (tasks[task_id].mes_done < tasks[task_id].n_steps) {
#endif
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
	do_next = (do_next + 1) % (int) tasks.size();
	int count = 0;
	while (tasks[do_next].is_done == 1 && count <= (int) (tasks.size())) {
		++count;
		do_next = (do_next + 1) % (int) tasks.size();
	}
	return (count >= (int) tasks.size()) ? -1 : do_next;
}

void runner_single::write() {
	odump d(masterfile);
	int dd = do_next - 1;
	d.write(dd);
	d.write<one_task>(tasks);
	d.close();
}

void runner_single::read() {
	idump d(masterfile);
	if (d.good()) {
		d.read(do_next);
		d.read<one_task>(tasks);
	} else
		do_next = -1;
	d.close();
	for (uint i = 0; i < taskfiles.size(); i++) {
		parser tp(taskfiles[i]);
		one_task new_task;
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
		(*STATUS) << 0 << " : Restart needed" << "\n";
	}
	report();
	(*STATUS) << 0 << " : finalized" << "\n";
	exit(0);
}

void runner_single::report() {
	for (uint i = 0; i < tasks.size(); i++) {
		(*STATUS) << tasks[i].task_id << "\t"
				<< int(tasks[i].mes_done / (double) (tasks[i].n_steps) * 100) << "%\t"
				<< tasks[i].steps_done << "\t" << tasks[i].mes_done << "\n";
	}
	(*STATUS) << "\n";
}

void runner_single::checkpointing() {
	cerr << "0 : C " << rundir << "\n";
	sys->_write(rundir);
	sys->measure.write(rundir);
	(*STATUS) << "0 : C " << rundir << "\n";
	cerr << "0 : C " << rundir << " done" << "\n";
}

void runner_single::merge_measurements() {
	cerr << "0 : M " << rundir << "\n";
	std::string mf = taskdir + ".out";
	sys->_write_output(mf);
	(*STATUS) << 0 << " : M " << taskdir << "\n";
	cerr << "0 : M " << rundir << " done" << "\n";
}

