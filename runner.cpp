#include "runner.h"
#include "merger.h"
#include <fmt/printf.h>
#include <experimental/filesystem>

runner::~runner()
{
	STATUS.close();
	delete sys;
}

int runner::start(const std::string& jobfile, double walltime, double checkpointtime, std::function<abstract_mc* (std::string &)> mccreator, int argc, char **argv)
{
	this->jobfile = jobfile;
	this->walltime = walltime;
	chktime = checkpointtime;
	my_mccreator = mccreator;

	MPI_Init(&argc,&argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	
	parser parsedfile(jobfile);

	taskfiles= parsedfile.return_vector<std::string>("@taskfiles");		
	time_start = MPI_Wtime();
	time_last_chkpt = time_start;
	statusfile = parsedfile.value_or_default<std::string>("statusfile",jobfile+".status");
	masterfile = parsedfile.value_or_default<std::string>("masterfile",jobfile+".master.h5");
	STATUS.open(statusfile.c_str(), std::ios::out|std::ios::app);

	if(my_rank == MASTER) {
		M_read();
		M_wait();
	}
	else what_is_next(S_IDLE);
	return 0;
}

bool runner::is_chkpt_time()
{
	if((MPI_Wtime() - time_last_chkpt)> chktime){
		time_last_chkpt = MPI_Wtime();
		return true;
	} 
	else return false;
}

bool runner::time_is_up()
{
	return ((MPI_Wtime() - time_start)>walltime);
}

int runner::M_get_new_task_id()
{
	next_task_id_ = (next_task_id_ + 1) % (int)tasks.size();
	int count = 0;
	while(tasks[next_task_id_].is_done == 1 && count <= (int)(tasks.size())) {
		++count;
		next_task_id_ = (next_task_id_ + 1) % (int)tasks.size();
	}
	return (count >= (int)tasks.size()) ? -1 : next_task_id_;
}

void runner::M_update(int node)
{
	int node_status;
	MPI_Status stat;
	MPI_Recv(&node_status, 1, MPI_INT, node, T_STATUS, MPI_COMM_WORLD, &stat);
	//STATUS << my_rank << ": Status " << node_status << " from " << node << "\n";
	if (node_status == S_IDLE) {
		if(time_is_up()) M_send_action(A_EXIT,node);
		else {
			int new_task_id = M_get_new_task_id();
			if(new_task_id<0) M_send_action(A_EXIT,node);
			else {
				M_send_action(A_NEW_JOB,node);
				tasks[new_task_id].run_counter++;
				char * ptr = (char*) &tasks[new_task_id];
				int task_comm_size = sizeof(runner_task) / sizeof(char);					
				MPI_Send(ptr, task_comm_size, MPI_CHAR, node, T_NEW_JOB, MPI_COMM_WORLD);
			}
		}
	}	
	else { // S_BUSY or  S_FINISHED
		runner_task act;
		int task_comm_size = sizeof(runner_task) / sizeof(char);
		MPI_Recv((char*) &act, task_comm_size, MPI_CHAR, node, T_STATUS, MPI_COMM_WORLD, &stat);
		tasks[act.task_id].steps_done += act.steps_done;
		tasks[act.task_id].mes_done += act.mes_done;
		if(tasks[act.task_id].mes_done >= tasks[act.task_id].n_steps) tasks[act.task_id].is_done = 1;
		if((tasks[act.task_id].is_done == 1) || (node_status == S_FINISHED)) {
			tasks[act.task_id].run_counter--;
			M_send_action((tasks[act.task_id].run_counter==0) ? 
				A_PROCESS_DATA_NEW_JOB : A_NEW_JOB, node);
		} 
		else M_send_action( ((time_is_up()) ? A_EXIT : A_CONTINUE), node);	
	}
	//M_write();    we will otherwise have too much redundant IO traffic
	//M_report();	we will otherwise have a lot of output!
}

void runner::M_send_action(int action, int to)
{
	//STATUS << my_rank << ": Action "<<action<<" to " << to << "\n";
	MPI_Send(&action, 1, MPI_INT, to, T_ACTION, MPI_COMM_WORLD);
	if(action == A_EXIT) N_exit ++;
}

void runner::M_wait()
{
	while (N_exit != world_size-1) {
		MPI_Status stat;
		int flag = 0;
		while(!flag) {
			if (is_chkpt_time()) M_write();
			MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &stat);
		}
		if(stat.MPI_TAG == T_STATUS)
			M_update(stat.MPI_SOURCE);
		else {
			fmt::printf("mpi rank {}: unknown message tag {} from {}\n", my_rank, stat.MPI_TAG, stat.MPI_SOURCE);
		}
	}
	M_write();
	M_end_of_run();
}

void runner::M_write()
{
	iodump d = iodump::create(masterfile);
	d.write("next_task_id", next_task_id_);

	d.change_group("tasks");
	for(const auto& task : tasks) {
		d.change_group(fmt::format("{:04d}", task.task_id));
		task.checkpoint_write(d);
		d.change_group("..");
	}
	d.change_group("..");
}

void runner::M_read()
{
	N_exit = 0;
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
		fmt::printf("runner::M_read: failed to read master file: {}. Discarding progress information.", e.what());
		next_task_id_= -1;
	}

	for(size_t i = 0; i < taskfiles.size(); i++){
		parser tp(taskfiles[i]);
		runner_task new_task;
		if (tp.defined("SWEEPS")) 
			new_task.n_steps=tp.return_value_of<int>("SWEEPS");
		if (tp.defined("n_steps")) 
			new_task.n_steps=tp.return_value_of<int>("n_steps");
		if (i<tasks.size()) {
			tasks[i].run_counter=0;
			if (new_task.n_steps  > tasks[i].n_steps) tasks[i].is_done=0;
			if (new_task.n_steps != tasks[i].n_steps) tasks[i].n_steps=new_task.n_steps;
		}
		else  {
			new_task.task_id = i;
			new_task.is_done = 0;
			new_task.steps_done = 0;
			new_task.mes_done = 0;
			new_task.run_counter = 0;
			tasks.push_back(new_task);
		}
	}
	M_report();
}

void runner::M_end_of_run()
{
	bool need_restart = false;
	for(uint i = 0; i < tasks.size(); i ++)
		if(tasks[i].is_done == 0) need_restart=true;
	if(need_restart) {
		std::string rfilename=jobfile+".restart";
		std::ofstream rfile(rfilename.c_str());
		rfile << "restart me\n";
		rfile.close();
		STATUS << my_rank << ": Restart needed" << "\n";
	}
	M_report();
 	MPI_Finalize();
	STATUS << my_rank << ": MPI finalized" << "\n";
	exit(0);
}

void runner::M_report() 
{
	for(uint i = 0; i < tasks.size(); i ++) {
		STATUS 	
		<< tasks[i].task_id << "\t" 
		<< tasks[i].run_counter << "\t"
		<< int(tasks[i].mes_done/(double)(tasks[i].n_steps)*100)<<"%\t"
		<< tasks[i].steps_done << "\t"
		<< tasks[i].mes_done << "\n";	
	}
	STATUS << "\n";
}

void runner::what_is_next(int status)
{
	MPI_Send(&status, 1, MPI_INT, MASTER, T_STATUS, MPI_COMM_WORLD);
	if (status==S_IDLE) {
		int new_action = recv_action();
		if (new_action == A_EXIT) end_of_run();
		MPI_Status stat;
		int task_comm_size = sizeof(runner_task) / sizeof(char);
		MPI_Recv((char*) &my_task, task_comm_size, MPI_CHAR, 0, T_NEW_JOB, MPI_COMM_WORLD, &stat);
		std::string taskfile = taskfiles[my_task.task_id];
		parser cfg(taskfile);
		my_taskdir = cfg.value_or_default("taskdir",fmt::format("task{:04d}", my_task.task_id+1));
		my_rundir = fmt::format("/run{:04d}", my_task.run_counter);
		my_count  = my_task.mes_done;
		run();
	}
	else {	
		int task_comm_size = sizeof(runner_task) / sizeof(char);
		MPI_Send((char*) &my_task, task_comm_size, MPI_CHAR, 0, T_STATUS, MPI_COMM_WORLD);
		int new_action = recv_action();
		if(new_action == A_PROCESS_DATA_NEW_JOB) {merge_measurements();what_is_next(S_IDLE);}
		if(new_action == A_NEW_JOB) what_is_next(S_IDLE);
		if(new_action == A_EXIT) end_of_run();
		//else, new_action == A_CONTINUE, and we 
		//continue from where we got here.
	}
}

int runner::recv_action()
{
	MPI_Status stat;
	int new_action;
	MPI_Recv(&new_action, 1, MPI_INT, MASTER, T_ACTION, MPI_COMM_WORLD, &stat);
	return new_action;
}

void runner::run()
{
	my_task.mes_done = 0;
	my_task.steps_done = 0;	
	delete sys;
	std::string taskfile = taskfiles[my_task.task_id];
	sys = my_mccreator(taskfile);
	if (sys->_read(my_rundir)) {
		STATUS << my_rank << ": L " << my_rundir  << "\n";
	}
	else {
		STATUS << my_rank << ": I " << my_rundir << "\n";
		sys->_init();
		checkpointing();
	}
	what_is_next(S_BUSY);
#ifdef NO_TASK_SHARE
	while (sys->work_done()<0.5)) {
#else
	while(my_count < my_task.n_steps) {	
#endif
		my_task.steps_done ++;
		sys->_do_update();
		if(sys->is_thermalized()) {
#ifndef NO_TASK_SHARE
			my_task.mes_done ++;
#endif
			my_count++;
			sys->do_measurement();
		}
		if(is_chkpt_time() || time_is_up()) {
			checkpointing();
			what_is_next(S_BUSY);
			my_task.mes_done = 0;
			my_task.steps_done = 0;	
		}
	}
	STATUS << my_rank << ": F " << my_rundir << "\n";
	checkpointing();
	what_is_next(S_FINISHED);	
}

void runner::checkpointing()
{
	sys->_write(my_rundir);
	STATUS << my_rank << ": C " << my_rundir << "\n";
}

void runner::merge_measurements() {
	if(my_task.run_counter == 1) {
		const std::string meas_ending = ".meas.h5";
		merger m;
		std::vector<std::string> meas_files;
		for(const auto &f : std::experimental::filesystem::directory_iterator("my_taskdir")) {
			std::string fname = f.path();
			if(fname.compare(fname.length()-meas_ending.length(), meas_ending.length(), meas_ending) == 0) {
				meas_files.push_back(fname);
			}
		}

		merger_results results = m.merge(meas_files);
		// TODO write out results
	}
	
	std::string mf=my_taskdir+".out";
	sys->_write_output(mf);
	STATUS << my_rank << ": M " << my_taskdir << "\n";
}

void runner::end_of_run()
{
 	MPI_Finalize();
	STATUS << my_rank << ": MPI finalized" << "\n";
	exit(0);
}

