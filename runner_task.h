#pragma once

class iodump;

// used by the runner
struct runner_task {
	int target_sweeps;
	int target_thermalization;
	int sweeps;
	int scheduled_runs;

	bool is_done() const;
	runner_task(int target_sweeps, int target_thermalization, int sweeps, int scheduled_runs);
};
