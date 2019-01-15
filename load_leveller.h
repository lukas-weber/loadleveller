#pragma once

#include "merger.h"
#include "runner.h"
#include "runner_single.h"

namespace load_leveller {

	template <class mc_runner>
	static int run_mc(std::function<abstract_mc * (const std::string&)> mccreator, int argc, char **argv) {
		if(argc < 4) {
			std::cerr << "Usage: " << argv[0] <<" jobfile walltime checkpointtime [h/m/s]\nThe last argument sets the time unit for walltime and checkpointtime. Default is seconds.\n";
			return -1;
		}

		std::string jobfile = argv[1];
		double walltime = atof(argv[2]);
		double chktime = atof(argv[3]);
		if(argc>4) {//default is seconds
			if(argv[4][0] == 'h') {
				walltime *= 3600;
				chktime *= 3600;
			}
			if(argv[4][0] == 'm') {
				walltime *= 60;
				chktime *= 60;
			}
		}

		mc_runner r;
		return r.start(jobfile, walltime, chktime, mccreator, argc, argv);
	}

	// run this function from main() in your code.
	template <class mc_implementation>
	int run(int argc, char **argv) {
		auto mccreator = [&] (const std::string& taskfile) -> abstract_mc* {return new mc_implementation(taskfile);};

		if(argc > 1 && std::string(argv[1]) == "merge") {
		//	return merge(mccreator, argc-1, argv+1);
		} else if(argc > 1 && std::string(argv[1]) == "single") {
			return run_mc<runner_single>(mccreator, argc-1, argv+1);
		}

		return run_mc<runner>(mccreator, argc, argv);
	}

}
