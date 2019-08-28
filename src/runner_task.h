#pragma once

namespace loadl {

// used by the runner
struct runner_task {
	int target_sweeps;
	int sweeps;
	int scheduled_runs;

	bool is_done() const;
	runner_task(int target_sweeps, int sweeps, int scheduled_runs);
};
}
