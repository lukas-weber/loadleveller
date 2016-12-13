#ifdef MCL_PT
#include "runner_pt.h"
#else
#ifdef MCL_SINGLE
#include "runner_single.h"
#else
#include "runner.h"
#endif
#endif
#include <fstream>
#include <iostream>
#include <string>
#include <cstdlib>
#include <ctime>

#include <sys/types.h>
#include <unistd.h>

using namespace std;

int main ( int argc, char *argv[] )
{
#ifdef MCL_PT
	runner_pt my_run(argc,argv);
#else
#ifdef MCL_SINGLE
	if(argc < 2)
	{
		cerr << "Usage: " << argv[0] <<" jobfile [walltime] [checkpointtime] "<< endl;
		return 1;
	}
	runner_single my_run(argc,argv);
#else
	runner my_run(argc,argv);
#endif
#endif
	my_run.start();
	return 0;
}
