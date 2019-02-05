#pragma once

#include "merger.h"
#include "runner.h"
#include "runner_single.h"
#include "mc.h"

namespace load_leveller {
	template <class mc_runner>
	static int run_mc(mc_factory mccreator, int argc, char **argv) {
		if(argc < 2) {
			std::cerr << fmt::format("{0} JOBFILE\n{0} single JOBFILE\n{0} merge JOBFILE\n\n Without further flags, the MPI scheduler is started. 'single' starts a single core scheduler (useful for debugging), 'merge' merges unmerged measurement results.\n", argv[0]);
			return -1;
		}

		std::string jobfile{argv[1]};

		mc_runner r;
		return r.start(jobfile, mccreator, argc, argv);
	}

	// run this function from main() in your code.
	template <class mc_implementation>
	int run(int argc, char **argv) {
		auto mccreator = [&] (const std::string& jobfile, const std::string& taskname) -> abstract_mc* {return new mc_implementation(jobfile, taskname);};

		if(argc > 1 && std::string(argv[1]) == "merge") {
		//	return merge(mccreator, argc-1, argv+1);
		} else if(argc > 1 && std::string(argv[1]) == "single") {
			return run_mc<runner_single>(mccreator, argc-1, argv+1);
		}

		return run_mc<runner_mpi>(mccreator, argc, argv);
	}

}
