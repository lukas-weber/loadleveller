#include "runner_task.h"
#include "iodump.h"

namespace loadl {

runner_task::runner_task(size_t target_sweeps, size_t sweeps, int scheduled_runs)
    : target_sweeps{target_sweeps}, sweeps{sweeps}, scheduled_runs{scheduled_runs} {}

bool runner_task::is_done() const {
	return sweeps >= target_sweeps;
}
}
