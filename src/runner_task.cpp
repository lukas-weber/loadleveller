#include "iodump.h"
#include "runner_task.h"

namespace loadl {

runner_task::runner_task(int target_sweeps, int target_thermalization, int sweeps,
                         int scheduled_runs)
    : target_sweeps{target_sweeps}, target_thermalization{target_thermalization}, sweeps{sweeps},
      scheduled_runs{scheduled_runs} {}

bool runner_task::is_done() const {
	return sweeps >= (target_sweeps + target_thermalization);
}
}
