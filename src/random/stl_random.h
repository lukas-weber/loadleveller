#pragma once

#include "iodump.h"
#include <random>
#include <sstream>

namespace loadl {

// based on the c++ stl implementation
template<typename stl_engine>
class rng_stl {
public:
	void backend_checkpoint_write(const iodump::group &dump_file) {
		std::stringstream buf;
		buf << generator_;
		buf << real_dist_;
		dump_file.write("state", buf.str());
	}
	void backend_checkpoint_read(const iodump::group &dump_file) {
		std::string state;
		dump_file.read("state", state);
		std::stringstream buf(state);
		buf >> generator_;
		buf >> real_dist_;
	}

	void set_seed(uint64_t seed) {
		generator_.seed(seed);
	}

	double random_double() {
		return real_dist_(generator_);
	}

	uint32_t random_integer(uint32_t bound) {
		std::uniform_int_distribution<uint32_t> int_dist(0, bound);
		return int_dist(generator_);
	}

private:
	stl_engine generator_;
	std::uniform_real_distribution<double> real_dist_{0, 1};
};

}
