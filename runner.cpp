#include "runner.h"
#include "merger.h"
#include <fmt/format.h>
#include <regex>
#include <dirent.h>

std::string runner::taskdir(int task_id) const {
	return fmt::format("{}.{}", jobfile_name_, task_names_.at(task_id));
}

std::string runner::rundir(int task_id, int run_id) const {
	return fmt::format("{}/run{:04d}", taskdir(task_id), run_id);
}


// This function lists files that could be run files being in the taskdir
// and having the right file_ending.
// The regex has to be matched with the output of the rundir function.
std::vector<std::string> runner::list_run_files(const std::string& taskdir, const std::string& file_ending) {
	std::regex run_filename{"run\\d{4,}."+file_ending};
	std::vector<std::string> results;
	DIR *dir = opendir(taskdir.c_str());
	struct dirent *result;
	while((result = readdir(dir)) != NULL) {
		std::string fname{result->d_name};
		if(std::regex_search(fname, run_filename)) {
			results.emplace_back(fmt::format("{}/{}", taskdir, fname));
		}
	}
	closedir(dir);

	return results;
}

int runner::start(const std::string& jobfile_name, const mc_factory& mccreator, int argc, char **argv)
{

	jobfile_name_ = jobfile_name;
	jobfile_ = std::unique_ptr<parser>{new parser{jobfile_name}};

	for(auto node : (*jobfile_)["tasks"]) {
		std::string task_name = node.first;
		if(std::find(task_names_.begin(), task_names_.end(), task_name) != task_names_.end()) {
			throw std::runtime_error(fmt::format("Task '{}' occured more than once in '{}'", task_name, jobfile_name));
		}
		task_names_.push_back(task_name);
	}

	// The jobconfig file contains information about the launch options, walltime, number of cores etc... not sure if this is really the best way to solve the issue.
	std::string jobconfig_path = jobfile_->get<std::string>("jobconfig");
	parser jobconfig{jobconfig_path};
	
	walltime_ = jobconfig.get<double>("mc_walltime");
	chktime_ = jobconfig.get<double>("mc_checkpoint_time");
	sweeps_before_communication_ = jobconfig.get<int>("mc_sweeps_before_communication", 10000);

	mccreator_ = mccreator;

	MPI_Init(&argc,&argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	
	time_start_ = MPI_Wtime();
	time_last_chkpt_ = time_start_;
	STATUS.open(fmt::format("{}.status", jobfile_name), std::ios::out|std::ios::app);

	if(my_rank == MASTER) {
		M_read();
		M_wait();
	}
	else what_is_next(S_IDLE);
	return 0;
}

bool runner::is_chkpt_time()
{
	if((MPI_Wtime() - time_last_chkpt_) > chktime_){
		time_last_chkpt_ = MPI_Wtime();
		return true;
	} 
	else return false;
}

bool runner::time_is_up()
{
	return (MPI_Wtime() - time_start_) > walltime_;
}

int runner::M_get_new_task_id(int old_id) {
	int ntasks = tasks_.size();
	int i;
	for(i = 1; i <= ntasks; i++) {
		if(!tasks_[(old_id+i)%ntasks].is_done())
			return (old_id+i)%ntasks;
	}

	// everything done!
	return -1;
}

void runner::M_update(int node)
{
	int node_status;
	MPI_Status stat;
	MPI_Recv(&node_status, 1, MPI_INT, node, T_STATUS, MPI_COMM_WORLD, &stat);
	//STATUS << my_rank << ": Status " << node_status << " from " << node << "\n";
	if (node_status == S_IDLE) {
		if(time_is_up()) {
			M_send_action(A_EXIT,node);
		} else {
			current_task_id_ = M_get_new_task_id(current_task_id_);
			
			if(current_task_id_ < 0) {
				M_send_action(A_EXIT,node);
			} else {
				M_send_action(A_NEW_JOB,node);
				tasks_[current_task_id_].scheduled_runs++;
				int msg[2] = { current_task_id_, tasks_[current_task_id_].scheduled_runs };
				MPI_Send(&msg, sizeof(msg)/sizeof(msg[0]), MPI_INT, node, T_NEW_JOB, MPI_COMM_WORLD);
			}
		}
	} else { // S_BUSY or  S_FINISHED
		int msg[2];
		MPI_Recv(msg, sizeof(msg)/sizeof(msg[0]), MPI_INT, node, T_STATUS, MPI_COMM_WORLD, &stat);
		int task_id = msg[0];
		int completed_sweeps = msg[1];
	      
		tasks_[task_id].sweeps += completed_sweeps;
		if(tasks_[task_id].is_done()) {
			tasks_[task_id].scheduled_runs--;
			M_send_action((tasks_[task_id].scheduled_runs == 0) ? 
				A_PROCESS_DATA_NEW_JOB : A_NEW_JOB, node);
		} else {
			M_send_action(time_is_up() ? A_EXIT : A_CONTINUE, node);	
		}
	}
	//M_report();	we will otherwise have a lot of output!
}

void runner::M_send_action(int action, int to)
{
	//STATUS << my_rank << ": Action "<<action<<" to " << to << "\n";
	MPI_Send(&action, 1, MPI_INT, to, T_ACTION, MPI_COMM_WORLD);
	if(action == A_EXIT) N_exit ++;
}

void runner::M_wait() {
	while (N_exit != world_size-1) {
		MPI_Status stat;
		int flag = 0;
		while(!flag) {
			MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &stat);
		}
		if(stat.MPI_TAG == T_STATUS)
			M_update(stat.MPI_SOURCE);
		else {
			STATUS << fmt::format("mpi rank {}: unknown message tag {} from {}\n", my_rank, stat.MPI_TAG, stat.MPI_SOURCE);
		}
	}
	M_end_of_run();
}

int runner::M_read_dump_progress(int task_id) {
	int sweeps = 0;
	for(auto& dump_name : list_run_files(taskdir(task_id), "dump.h5")) {
		int dump_sweeps;
		iodump d = iodump::open_readonly(dump_name);
		d.get_root().read("sweeps", dump_sweeps);
		sweeps += dump_sweeps;
	}

	return sweeps;
}

void runner::M_read() {
	for(size_t i = 0; i < task_names_.size(); i++) {
		auto task = (*jobfile_)["tasks"][task_names_[i]];
		
		int target_sweeps = task.get<int>("sweeps");
		int target_thermalization = task.get<int>("thermalization");
		int sweeps = M_read_dump_progress(i);
		int scheduled_runs = 0;
		
		tasks_.emplace_back(target_sweeps, target_thermalization, sweeps, scheduled_runs);
	}
	M_report();
}

void runner::M_end_of_run()
{
	bool need_restart = false;
	for(size_t i = 0; i < tasks_.size(); i ++) {
		if(!tasks_[i].is_done()) {
			need_restart=true;
			break;
		}
	}
	
	if(need_restart) {
		std::string rfilename = jobfile_name_+".restart";
		std::ofstream rfile(rfilename);
		rfile << "restart me\n";
		rfile.close();
		STATUS << my_rank << ": Restart needed" << "\n";
	}
	M_report();
 	MPI_Finalize();
	STATUS << my_rank << ": MPI finalized" << "\n";
	exit(0);
}

void runner::M_report() {
	for(size_t i = 0; i < tasks_.size(); i ++) {
		STATUS << fmt::format("{:4d} {:4d} {:3.0f}%\n", i, tasks_[i].scheduled_runs,
				      tasks_[i].sweeps/(double)(tasks_[i].target_sweeps + tasks_[i].target_thermalization)*100);
	}
}

void runner::what_is_next(int status) {
	MPI_Send(&status, 1, MPI_INT, MASTER, T_STATUS, MPI_COMM_WORLD);
	if (status==S_IDLE) {
		int new_action = recv_action();
		if(new_action == A_EXIT)
			end_of_run();
		MPI_Status stat;
		int msg[3];
		MPI_Recv(&msg, sizeof(msg)/sizeof(msg[0]), MPI_INT, 0, T_NEW_JOB, MPI_COMM_WORLD, &stat);
		task_id_ = msg[0];
		run_id_ = msg[1];

		run();
	} else {	
		int msg[2] = { current_task_id_, sys->sweep() };
		MPI_Send(msg, sizeof(msg)/sizeof(msg[0]), MPI_INT, 0, T_STATUS, MPI_COMM_WORLD);
		int new_action = recv_action();
		if(new_action == A_PROCESS_DATA_NEW_JOB) {
			merge_measurements();
			what_is_next(S_IDLE);
		} else if(new_action == A_NEW_JOB) {
			what_is_next(S_IDLE);
		} else if(new_action == A_EXIT) {
			end_of_run();
		}
		//else, new_action == A_CONTINUE, and we 
		//continue from where we got here.
	}
}

int runner::recv_action() {
	MPI_Status stat;
	int new_action;
	MPI_Recv(&new_action, 1, MPI_INT, MASTER, T_ACTION, MPI_COMM_WORLD, &stat);
	return new_action;
}

void runner::run() {
	sys = std::unique_ptr<abstract_mc>{mccreator_(jobfile_name_, task_names_[task_id_])};
	if (sys->_read(rundir(task_id_, run_id_))) {
		STATUS << my_rank << ": L " << rundir(task_id_, run_id_)  << "\n";
	} else {
		STATUS << my_rank << ": I " << rundir(task_id_, run_id_) << "\n";
		sys->_init();
		checkpointing();
	}
	what_is_next(S_BUSY);
	while(sys->sweep() < sweeps_before_communication_) {	
		sys->_do_update();
		if(sys->is_thermalized()) { // TODO
			sys->do_measurement();
		}
		if(is_chkpt_time() || time_is_up()) {
			checkpointing();
			what_is_next(S_BUSY);
		}
	}
	checkpointing();
	what_is_next(S_BUSY);
}

void runner::checkpointing()
{
	sys->_write(rundir(task_id_, run_id_));
	STATUS << my_rank << ": C " << rundir(task_id_, run_id_) << "\n";
}

void runner::merge_measurements() {
	if(run_id_ == 1) {
		merger m;
		std::vector<std::string> meas_files = list_run_files(task_names_[task_id_], ".meas.h5");

		results results = m.merge(meas_files);
		// TODO write out results
	}
	
	std::string mf=taskdir(task_id_)+".out";
	sys->_write_output(mf);
	STATUS << my_rank << ": M " << taskdir(task_id_) << "\n";
}

void runner::end_of_run()
{
 	MPI_Finalize();
	STATUS << my_rank << ": MPI finalized" << "\n";
	exit(0);
}

