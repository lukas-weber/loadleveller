#include "random.h"

#ifdef MCL_RNG_MT
randomnumbergenerator::randomnumbergenerator() {
	seed_ = mtrand_.get_seed();
}

randomnumbergenerator::randomnumbergenerator(uint64_t seed) : mtrand_{seed}, seed_{seed} {}

void randomnumbergenerator::checkpoint_write(const iodump::group &d) {
	std::vector<uint64_t> randState;
	mtrand_.save(randState);
	// if you implement your own random number generator backend,
	// make sure to keep this file layout for scripts that like to read the seed.
	d.write("state", randState);
	d.write("seed", seed_);

	// This is for future compatibility. B) You can add RNG implementations without
	// breaking compatibility to old runs.
	d.write("type", static_cast<uint32_t>(RNG_MersenneTwister));
}

void randomnumbergenerator::checkpoint_read(const iodump::group &d) {
	std::vector<uint64_t> randState;
	d.read("state", randState);
	d.read("seed", seed_);
	mtrand_.load(randState);
}

double randomnumbergenerator::norm() { // adjusted from https://de.wikipedia.org/wiki/Polar-Methode
	/* for now, I do not need the extra speed coming from this.
	 * If you add hidden state like this you have to change the dump format
	 * (breaking compatibility with your old runs)
	   if(norm_is_stored) {
	    norm_is_stored=false;
	    return norm_stored;
	}
	*/
	double u, v, q, p;

	do {
		u = 2.0 * d() - 1;
		v = 2.0 * d() - 1;
		q = u * u + v * v;
	} while(q <= 0.0 || q >= 1.0);

	p = sqrt(-2 * log(q) / q);
	/*
	norm_stored = u * p;
	norm_is_stored = true;
	*/
	return v * p;
}

#endif

#ifdef MCL_RNG_SPRNG_4
randomnumbergenerator::randomnumbergenerator() {
	int gtype = 4; // Multipl. Lagg. Fib.
	ptr = SelectType(gtype);
	seed_ = make_sprng_seed();
	ptr->init_sprng(0, 1, seed_, SPRNG_DEFAULT);
}

randomnumbergenerator::randomnumbergenerator(luint seed) {
	int gtype = 4; // Multipl. Lagg. Fib.
	ptr = SelectType(gtype);
	seed_ = seed;
	ptr->init_sprng(0, 1, seed_, SPRNG_DEFAULT);
}

void randomnumbergenerator::write(odump &d) {
	char *buffer;
	int buffersize = ptr->pack_sprng(&buffer);
	d.write(buffersize);
	d.write(buffer, buffersize);
	d.write(seed_);
	delete buffer;
}

void randomnumbergenerator::read(idump &d) {
	char *buffer;
	int buffersize;
	d.read(buffersize);
	buffer = new char[buffersize];
	d.read(buffer, buffersize);
	d.read(seed_);
	ptr->unpack_sprng(buffer);
	delete buffer;
}
#endif

#ifdef MCL_RNG_BOOST
void randomnumbergenerator::write(odump &d) {}

void randomnumbergenerator::read(idump &d) {}

randomnumbergenerator::randomnumbergenerator() {
	rng = new boost::mt19937;
	val = new boost::uniform_real<>(0.0, 1.0);

	seed_ = 5489;
}

randomnumbergenerator::randomnumbergenerator(int seed) {
	rng = new boost::mt19937;
	rng->seed(seed);
	val = new boost::uniform_real<>(0.0, 1.0);

	seed_ = seed;
}
#endif
