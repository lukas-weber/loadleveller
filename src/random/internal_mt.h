#pragma once

#include "mt19937.h"
#include "iodump.h"

namespace loadl {
	
class rng_internal_mersenne {
private:
	mt19937 mtrand_;

public:
	void backend_checkpoint_write(const iodump::group &dump_file) {
		std::vector<uint64_t> rand_state;
		mtrand_.save(rand_state);
		dump_file.write("state", rand_state);
	}

	void backend_checkpoint_read(const iodump::group &dump_file) {
		std::vector<uint64_t> rand_state;
		dump_file.read("state", rand_state);
		mtrand_.load(rand_state);
	}
	void set_seed(uint64_t seed) {
		mtrand_.seed(seed);
	}

	double random_double() {
		return mtrand_.rng_double();
	}

	int random_integer(int bound) {
		return mtrand_.rng_integer(bound);
	}
};

}
