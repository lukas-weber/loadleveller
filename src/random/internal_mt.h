#pragma once

#include "MersenneTwister.h"
#include "iodump.h"

namespace loadl {

// based on a dinosaur code in the MersenneTwister.h header
class rng_internal_mersenne {
private:
	MTRand mtrand_;

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
		return mtrand_.randDblExc(1);
	}

	int random_integer(int bound) {
		return mtrand_.randInt(bound - 1);
	}
};

}
