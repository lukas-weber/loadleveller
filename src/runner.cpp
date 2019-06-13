#include "runner.h"
#include "merger.h"
#include <dirent.h>
#include <fmt/format.h>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sys/stat.h>

namespace loadl {

enum {
	MASTER = 0,
	T_STATUS = 1,
	T_ACTION = 2,
	T_NEW_JOB = 3,

	S_IDLE = 1,
	S_BUSY = 2,

	A_EXIT = 1,
	A_CONTINUE = 2,
	A_NEW_JOB = 3,
	A_PROCESS_DATA_NEW_JOB = 4,
};

// parses the duration '[[hours:]minutes:]seconds' into seconds
// replace as soon as there is an alternative
static int parse_duration(const std::string &str) {
	size_t idx;

	try {
		int i1 = std::stoi(str, &idx, 10);
		if(idx == str.size()) {
			return i1;
		} else if(str[idx] == ':') {
			std::string str1 = str.substr(idx+1);
			int i2 = std::stoi(str1, &idx, 10);

			if(idx == str1.size()) {
				return 60 * i1 + i2;
			} else if(str[idx] == ':') {
				std::string str2 = str1.substr(idx+1);
				int i3 = std::stoi(str2, &idx, 10);
				if(idx != str2.size()) {
					throw std::runtime_error{"minutes"};
				}

				return 60 * 60 * i1 + 60 * i2 + i3;
			} else {
				throw std::runtime_error{"hours"};
			}
		} else {
			throw std::runtime_error{"seconds"};
		}
	} catch(std::exception &e) {
		throw std::runtime_error{
		    fmt::format("'{}' does not fit time format [[hours:]minutes:]seconds: {}", str, e.what())};
	}
}

std::string jobinfo::taskdir(int task_id) const {
	return fmt::format("{}.data/{}", jobname, task_names.at(task_id));
}

std::string jobinfo::rundir(int task_id, int run_id) const {
	return fmt::format("{}/run{:04d}", taskdir(task_id), run_id);
}

jobinfo::jobinfo(const std::string &jobfile_name) : jobfile{jobfile_name} {
	for(auto node : jobfile["tasks"]) {
		std::string task_name = node.first;
		task_names.push_back(task_name);
	}
	std::sort(task_names.begin(), task_names.end());

	jobname = jobfile.get<std::string>("jobname");

	std::string datadir = fmt::format("{}.data", jobname);
	int rc = mkdir(datadir.c_str(), 0755);
	if(rc != 0 && errno != EEXIST) {
		throw std::runtime_error{
		    fmt::format("creation of output directory '{}' failed: {}", datadir, strerror(errno))};
	}

	// perhaps a bit controversally, jobinfo tries to create the task directories. TODO: single file
	// output.
	for(size_t i = 0; i < task_names.size(); i++) {
		int rc = mkdir(taskdir(i).c_str(), 0755);
		if(rc != 0 && errno != EEXIST) {
			throw std::runtime_error{fmt::format("creation of output directory '{}' failed: {}",
			                                     taskdir(i), strerror(errno))};
		}
	}

	parser jobconfig{jobfile["jobconfig"]};

	walltime = parse_duration(jobconfig.get<std::string>("mc_walltime"));
	checkpoint_time = parse_duration(jobconfig.get<std::string>("mc_checkpoint_time"));
}

// This function lists files that could be run files being in the taskdir
// and having the right file_ending.
// The regex has to be matched with the output of the rundir function.
std::vector<std::string> jobinfo::list_run_files(const std::string &taskdir,
                                                 const std::string &file_ending) {
	std::regex run_filename{"run\\d{4,}\\." + file_ending};
	std::vector<std::string> results;
	DIR *dir = opendir(taskdir.c_str());
	if(dir == nullptr) {
		throw std::ios_base::failure(
		    fmt::format("could not open directory '{}': {}", taskdir, strerror(errno)));
	}
	struct dirent *result;
	while((result = readdir(dir)) != nullptr) {
		std::string fname{result->d_name};
		if(std::regex_search(fname, run_filename)) {
			results.emplace_back(fmt::format("{}/{}", taskdir, fname));
		}
	}
	closedir(dir);

	return results;
}

void jobinfo::concatenate_results() {
	std::ofstream cat_results{fmt::format("{}.results.yml", jobname)};
	for(size_t i = 0; i < task_names.size(); i++) {
		std::ifstream res_file{taskdir(i) + "/results.yml"};
		res_file.seekg(0, res_file.end);
		size_t size = res_file.tellg();
		res_file.seekg(0, res_file.beg);

		std::vector<char> buf(size + 1, 0);
		res_file.read(buf.data(), size);
		cat_results << buf.data() << "\n";
	}
}

void jobinfo::merge_task(int task_id, const std::vector<evalable> &evalables) {
	std::vector<std::string> meas_files = list_run_files(taskdir(task_id), "meas\\.h5");
	results results = merge(meas_files, evalables);

	std::string result_filename = fmt::format("{}/results.yml", taskdir(task_id));
	const std::string &task_name = task_names.at(task_id);
	results.write_yaml(result_filename, taskdir(task_id), jobfile["tasks"][task_name].get_yaml());
}

void jobinfo::log(const std::string &message) {
	std::time_t t = std::time(nullptr);
	std::cout << std::put_time(std::localtime(&t), "%F %T: ") << message << "\n";
}

int runner_mpi_start(jobinfo job, const mc_factory &mccreator, int argc, char **argv) {
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
	time_start_ = MPI_Wtime();
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

bool runner_master::time_is_up() const {
	return MPI_Wtime() - time_start_ > job_.walltime;
}

void runner_master::react() {
	int node_status;
	MPI_Status stat;
	MPI_Recv(&node_status, 1, MPI_INT, MPI_ANY_SOURCE, T_STATUS, MPI_COMM_WORLD, &stat);
	int node = stat.MPI_SOURCE;
	if(node_status == S_IDLE) {
		if(time_is_up()) {
			send_action(A_EXIT, node);
		} else {
			current_task_id_ = get_new_task_id(current_task_id_);

			if(current_task_id_ < 0) {
				send_action(A_EXIT, node);
				num_active_ranks_--;
			} else {
				send_action(A_NEW_JOB, node);
				tasks_[current_task_id_].scheduled_runs++;
				int msg[3] = {current_task_id_, tasks_[current_task_id_].scheduled_runs,
				              tasks_[current_task_id_].target_sweeps};
				MPI_Send(&msg, sizeof(msg) / sizeof(msg[0]), MPI_INT, node, T_NEW_JOB,
				         MPI_COMM_WORLD);
			}
		}
	} else { // S_BUSY
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
		} else if(time_is_up()) {
			send_action(A_EXIT, node);
			num_active_ranks_--;
		} else {
			send_action(A_CONTINUE, node);
		}
	}

}

void runner_master::send_action(int action, int destination) {
	MPI_Send(&action, 1, MPI_INT, destination, T_ACTION, MPI_COMM_WORLD);
}

int runner_master::read_dump_progress(int task_id) {
	int sweeps = 0;
	try {
		for(auto &dump_name : jobinfo::list_run_files(job_.taskdir(task_id), "dump\\.h5")) {
			int dump_sweeps = 0;
			iodump d = iodump::open_readonly(dump_name);
			d.get_root().read("sweeps", dump_sweeps);
			sweeps += dump_sweeps;
		}
	} catch(iodump_exception &e) {
		// okay
	} catch(std::ios_base::failure &e) {
		// might happen if the taskdir does not exist
	}

	return sweeps;
}

void runner_master::read() {
	for(size_t i = 0; i < job_.task_names.size(); i++) {
		auto task = job_.jobfile["tasks"][job_.task_names[i]];

		int target_sweeps = task.get<int>("sweeps");
		int target_thermalization = task.get<int>("thermalization");
		int sweeps = read_dump_progress(i);
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
				// checkpointing();
				job_.log(fmt::format("* initialized {}", job_.rundir(task_id_, run_id_)));
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
		checkpointing();
		action = what_is_next(S_BUSY);
	}
	if(time_is_up()) {
		job_.log(fmt::format("rank {} exits: time limit reached", rank_));
	} else {
		job_.log(fmt::format("rank {} exits: out of work", rank_));
	}
}

bool runner_slave::is_checkpoint_time() {
	return MPI_Wtime() - time_last_checkpoint_ > job_.checkpoint_time;
}

bool runner_slave::time_is_up() {
	return MPI_Wtime() - time_start_ > job_.walltime;
}

int runner_slave::what_is_next(int status) {
	MPI_Send(&status, 1, MPI_INT, MASTER, T_STATUS, MPI_COMM_WORLD);
	if(status == S_IDLE) {
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

void runner_slave::checkpointing() {
	time_last_checkpoint_ = MPI_Wtime();
	sys_->_write(job_.rundir(task_id_, run_id_));
	job_.log(fmt::format("* rank {}: checkpointing {}", rank_, job_.rundir(task_id_, run_id_)));
}

void runner_slave::merge_measurements() {
	std::string unique_filename = job_.taskdir(task_id_);
	sys_->_write_output(unique_filename);

	std::vector<evalable> evalables;
	sys_->register_evalables(evalables);
	job_.merge_task(task_id_, evalables);
}
}
