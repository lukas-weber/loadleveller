#pragma once

#include <vector>
#include <string>
#include "parser.h"
#include "iodump.h"
#include "evalable.h"

namespace loadl {

struct jobinfo {
	parser jobfile;
	std::string jobname;

	std::vector<std::string> task_names;

	double checkpoint_time;
	double walltime;

	jobinfo(const std::string &jobfile_name);

	std::string jobdir() const;
	std::string rundir(int task_id, int run_id) const;
	std::string taskdir(int task_id) const;

	static std::vector<std::string> list_run_files(const std::string &taskdir,
	                                               const std::string &file_ending);
	int read_dump_progress(int task_id) const;
	void merge_task(int task_id, const std::vector<evalable> &evalables);
	void concatenate_results();
	void log(const std::string &message);
};

}
