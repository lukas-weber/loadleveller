#pragma once

class iodump;

// used by the runner
struct runner_task {
	int task_id = 0;
	int is_done = 0;
	int n_steps = 0;
	int steps_done = 0;
	int mes_done = 0;
	int run_counter = 0; 

	void checkpoint_write(iodump &d) const;
	void checkpoint_read(iodump &d);
};
