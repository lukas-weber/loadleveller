#pragma once

#include <loadleveller/loadleveller.h>
#include <string>
#include <vector>

class silly_mc : public loadl::mc {
private:
	uint64_t idx_;

public:
	void init();
	void do_update();
	void do_measurement();
	void checkpoint_write(const loadl::iodump::group &out);
	void checkpoint_read(const loadl::iodump::group &in);

	void register_evalables(loadl::evaluator &eval);

	silly_mc(const loadl::parser &p);
};
