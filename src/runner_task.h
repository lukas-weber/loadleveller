#pragma once

#include <cstddef>

namespace loadl {

// used by the runner
struct runner_task {
	size_t target_sweeps;
	size_t sweeps;
	int scheduled_runs;

	bool is_done() const;
	runner_task(size_t target_sweeps, size_t sweeps, int scheduled_runs);
};
}
