#pragma once

#include "merger.h"
#include "runner.h"
#include "runner_single.h"
#include "mc.h"

namespace load_leveller {
	inline int merge_only(jobinfo job, const mc_factory& mccreator, int argc, char **argv) {
		for(size_t task_id = 0; task_id < job.task_names.size(); task_id++) {
			std::vector<evalable> evalables;
			std::unique_ptr<abstract_mc> sys{mccreator(job.jobfile_name, job.task_names[task_id])};
			sys->register_evalables(evalables);
			job.merge_task(task_id, evalables);

			std::cout << fmt::format("-- {} merged\n", job.taskdir(task_id));
		}

		return 0;
	}
	
	inline int run_mc(int (*starter)(jobinfo job, const mc_factory&, int argc, char **argv), mc_factory mccreator, int argc, char **argv) {
		if(argc < 2) {
			std::cerr << fmt::format("{0} JOBFILE\n{0} single JOBFILE\n{0} merge JOBFILE\n\n Without further flags, the MPI scheduler is started. 'single' starts a single core scheduler (useful for debugging), 'merge' merges unmerged measurement results.\n", argv[0]);
			return -1;
		}

		std::string jobfile{argv[1]};
		jobinfo job{jobfile};

		return starter(std::move(job), mccreator, argc, argv);
	}

	// run this function from main() in your code.
	template <class mc_implementation>
	int run(int argc, char **argv) {
		auto mccreator = [&] (const std::string& jobfile, const std::string& taskname) -> abstract_mc* {return new mc_implementation(jobfile, taskname);};

		if(argc > 1 && std::string(argv[1]) == "merge") {
			
			return run_mc(merge_only, mccreator, argc-1, argv+1);
		} else if(argc > 1 && std::string(argv[1]) == "single") {
			return run_mc(runner_single_start, mccreator, argc-1, argv+1);
		}

		return run_mc(runner_mpi_start, mccreator, argc, argv);
	}
}
