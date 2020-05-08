#pragma once

#include "evalable.h"
#include "iodump.h"
#include "parser.h"
#include <string>
#include <vector>

namespace loadl {

using register_evalables_func = std::function<void (evaluator &, const parser &)>;

class jobinfo {
private:
	register_evalables_func evalable_func_;
public:
	parser jobfile;
	std::string jobname;

	std::vector<std::string> task_names;

	double checkpoint_time{};
	double runtime{};

	jobinfo(const std::string &jobfile_name, register_evalables_func evalable_func);

	std::string jobdir() const;
	std::string rundir(int task_id, int run_id) const;
	std::string taskdir(int task_id) const;

	static std::vector<std::string> list_run_files(const std::string &taskdir,
	                                               const std::string &file_ending);
	int read_dump_progress(int task_id) const;
	void merge_task(int task_id);
	void concatenate_results();
	void log(const std::string &message);
};

}
