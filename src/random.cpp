#include "random.h"

namespace loadl {

void rng_internal_mersenne::set_seed(uint64_t seed) {
	mtrand_.seed(seed);
}

void rng_internal_mersenne::backend_checkpoint_write(const iodump::group &d) {
	std::vector<uint64_t> rand_state;
	mtrand_.save(rand_state);
	d.write("state", rand_state);
}

void rng_internal_mersenne::backend_checkpoint_read(const iodump::group &d) {
	std::vector<uint64_t> rand_state;
	d.read("state", rand_state);
	mtrand_.load(rand_state);
}

double rng_internal_mersenne::random_double() {
	return mtrand_.randDblExc(1);
}

int rng_internal_mersenne::random_integer(int bound) {
	return mtrand_.randInt(bound - 1);
}

}
