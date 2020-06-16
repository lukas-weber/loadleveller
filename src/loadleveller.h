#pragma once

#include "mc.h"
#include "merger.h"
#include "runner.h"
#include "runner_single.h"
namespace loadl {

inline int merge_only(jobinfo job, const mc_factory &, int, char **) {
	int ntasks = job.task_names.size();
	int len = log(ntasks) / log(10) + 1;
	size_t max_tasklen = 0;

	for(size_t task_id = 0; task_id < job.task_names.size(); task_id++) {
		std::string taskdir = job.taskdir(task_id);
		max_tasklen = std::max(max_tasklen, taskdir.size());

		std::cout << fmt::format("\rMerging task {0: >{3}}/{1: >{3}}... {2: <{4}}", task_id + 1,
		                         job.task_names.size(), taskdir, len, max_tasklen);
		std::cout.flush();

		job.merge_task(task_id);
	}
	std::cout << fmt::format("\rMerged {0} tasks.{1: >{2}}\n", job.task_names.size(), "",
	                         2 * len + 5 + max_tasklen);

	return 0;
}

template<typename mc_implementation>
int run_mc(int (*starter)(jobinfo job, const mc_factory &, int argc, char **argv), int argc,
           char **argv) {
	if(argc < 2) {
		std::cerr << fmt::format(
		    "{0} JOBFILE\n{0} single JOBFILE\n{0} merge JOBFILE\n\n Without further flags, the MPI "
		    "scheduler is started. 'single' starts a single core scheduler (useful for debugging), "
		    "'merge' merges unmerged measurement results.\n",
		    argv[0]);
		return -1;
	}

	std::string jobfile{argv[1]};
	auto mccreator = [&](const parser &p) -> mc * { return new mc_implementation{p}; };

	jobinfo job{jobfile, &mc_implementation::register_evalables};

	// bad hack because hdf5 locking features will happily kill your
	// production run in the middle of writing measurements if you block
	// a writing lock by reading some measurement files.
	setenv("HDF5_USE_FILE_LOCKING", "FALSE", 1);

	int rc = starter(job, mccreator, argc, argv);

	job.concatenate_results();
	return rc;
}

// run this function from main() in your code.
template<class mc_implementation>
int run(int argc, char **argv) {
	if(argc > 1 && std::string(argv[1]) == "merge") {
		return run_mc<mc_implementation>(merge_only, argc - 1, argv + 1);
	} else if(argc > 1 && std::string(argv[1]) == "single") {
		return run_mc<mc_implementation>(runner_single_start, argc - 1, argv + 1);
	}

	return run_mc<mc_implementation>(runner_mpi_start, argc, argv);
}
}
