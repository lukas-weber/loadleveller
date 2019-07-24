#pragma once

#include "mc.h"
#include "jobinfo.h"
#include "runner_task.h"
#include <functional>
#include <ostream>

namespace loadl {

int runner_single_start(jobinfo job, const mc_factory &mccreator, int argc, char **argv);

class runner_single {
private:
	jobinfo job_;

	mc_factory mccreator_;
	std::unique_ptr<mc> sys_;

	int task_id_{-1};
	std::vector<runner_task> tasks_;

	double time_start_{0};
	double time_last_checkpoint_{0};

	void read();
	int get_new_task_id(int old_id);

	bool time_is_up() const;
	bool is_checkpoint_time() const;

	void checkpointing();
	void merge_measurements();

public:
	runner_single(jobinfo, mc_factory mccreator);
	int start();
};
}
