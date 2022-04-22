#include "jobinfo.h"
#include "mc.h"
#include "merger.h"
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>

namespace loadl {

// parses the duration '[[hours:]minutes:]seconds' into seconds
// replace as soon as there is an alternative
static int parse_duration(const std::string &str) {
	size_t idx;

	try {
		int i1 = std::stoi(str, &idx, 10);
		if(idx == str.size()) {
			return i1;
		} else if(str[idx] == ':') {
			std::string str1 = str.substr(idx + 1);
			int i2 = std::stoi(str1, &idx, 10);

			if(idx == str1.size()) {
				return 60 * i1 + i2;
			} else if(str1[idx] == ':') {
				std::string str2 = str1.substr(idx + 1);
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
		throw std::runtime_error{fmt::format(
		    "'{}' does not fit time format [[hours:]minutes:]seconds: {}", str, e.what())};
	}
}

std::filesystem::path jobinfo::taskdir(int task_id) const {
	return jobdir / task_names.at(task_id);
}

std::filesystem::path jobinfo::rundir(int task_id, int run_id) const {
	return taskdir(task_id) / fmt::format("run{:04d}", run_id);
}

jobinfo::jobinfo(const std::filesystem::path &jobfile_name, register_evalables_func evalable_func)
    : evalable_func_{evalable_func}, jobfile{jobfile_name}, jobdir{jobfile_name.parent_path()} {
	for(auto node : jobfile["tasks"]) {
		std::string task_name = node.first;
		task_names.push_back(task_name);
	}
	std::sort(task_names.begin(), task_names.end());

	jobname = jobfile.get<std::string>("jobname");

	std::error_code ec;
	std::filesystem::create_directories(jobdir, ec);

	// perhaps a bit controversally, jobinfo tries to create the task directories. TODO: single file
	// output.
	for(size_t i = 0; i < task_names.size(); i++) {
		std::filesystem::create_directories(taskdir(i));
	}

	parser jobconfig{jobfile["jobconfig"]};

	runtime = parse_duration(jobconfig.get<std::string>("mc_runtime"));
	checkpoint_time = parse_duration(jobconfig.get<std::string>("mc_checkpoint_time"));
}

// This function lists files that could be run files being in the taskdir
// and having the right file_ending.
// The regex has to be matched with the output of the rundir function.
std::vector<std::filesystem::path> jobinfo::list_run_files(const std::string &taskdir,
                                                           const std::string &file_ending) {
	std::regex run_filename{"^run\\d{4,}\\." + file_ending + "$"};
	std::vector<std::filesystem::path> results;

	for(const auto &p : std::filesystem::directory_iterator(taskdir)) {
		if(std::regex_search(p.path().filename().string(), run_filename)) {
			results.emplace_back(p.path());
		}
	}

	return results;
}

size_t jobinfo::read_dump_progress(int task_id) const {
	size_t sweeps = 0;
	try {
		for(auto &dump_name : list_run_files(taskdir(task_id), "dump\\.h5")) {
			size_t dump_sweeps = 0;
			iodump d = iodump::open_readonly(dump_name);
			d.get_root().read("sweeps", dump_sweeps);
			sweeps += dump_sweeps;
		}
	} catch(std::ios_base::failure &e) {
		// might happen if the taskdir does not exist
	}

	return sweeps;
}

void jobinfo::concatenate_results() {
	std::ofstream cat_results{jobdir.parent_path() / fmt::format("{}.results.json", jobname)};
	cat_results << "[";
	for(size_t i = 0; i < task_names.size(); i++) {
		std::ifstream res_file{taskdir(i) / "results.json"};
		res_file.seekg(0, res_file.end);
		size_t size = res_file.tellg();
		res_file.seekg(0, res_file.beg);

		std::vector<char> buf(size + 1, 0);
		res_file.read(buf.data(), size);
		cat_results << buf.data();
		if(i < task_names.size() - 1) {
			cat_results << ",";
		}
		cat_results << "\n";
	}
	cat_results << "]\n";
}

void jobinfo::merge_task(int task_id) {
	std::vector<std::filesystem::path> meas_files = list_run_files(taskdir(task_id), "meas\\.h5");
	size_t rebinning_bin_length = jobfile["jobconfig"].get<size_t>("merge_rebin_length", 0);
	size_t sample_skip = jobfile["jobconfig"].get<size_t>("merge_sample_skip", 0);
	results results = merge(meas_files, rebinning_bin_length, sample_skip);

	evaluator eval{results};
	evalable_func_(eval, jobfile["tasks"][task_names[task_id]]);
	eval.append_results();

	std::filesystem::path result_filename = taskdir(task_id) / "results.json";
	const std::string &task_name = task_names.at(task_id);
	results.write_json(result_filename, taskdir(task_id), jobfile["tasks"][task_name].get_json());
}

void jobinfo::log(const std::string &message) {
	std::time_t t = std::time(nullptr);
	std::cout << std::put_time(std::localtime(&t), "%F %T: ") << message << "\n";
}

}
