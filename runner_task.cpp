#include "runner.h"
#include "dump.h"

void runner_task::checkpoint_write(iodump &d) const {
	d.write("task_id", task_id);
	d.write("is_done", is_done);
	d.write("n_steps", n_steps);
	d.write("steps_done", steps_done);
	d.write("mes_done", mes_done);
	d.write("run_counter", run_counter);
}

void runner_task::checkpoint_read(iodump &d) {
	d.read("task_id", task_id);
	d.read("is_done", is_done);
	d.read("n_steps", n_steps);
	d.read("steps_done", steps_done);
	d.read("mes_done", mes_done);
	d.read("run_counter", run_counter);
}
	
