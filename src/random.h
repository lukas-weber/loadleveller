#pragma once

#include "config.h"

#include "MersenneTwister.h"
#include "dump.h"
#include <random>
#include <sstream>
#include <typeinfo>

// A whole bunch of template magic to make the random backend modular.
// We don’t want a vtable on such a performance critical object, otherwise I
// would make it settable at runtime.

namespace loadl {

template<class base>
class rng_base : public base {
private:
	static uint32_t get_rank() {
		int initialized;
		MPI_Initialized(&initialized);
		if(initialized) {
			int rank;
			MPI_Comm_rank(MPI_COMM_WORLD, &rank);
			return rank;
		}
		return 0;
	}

	uint64_t seed_;

public:
	rng_base() {
		std::seed_seq seq = {static_cast<uint64_t>(time(0)), static_cast<uint64_t>(clock()),
		                     static_cast<uint64_t>(get_rank())};
		std::vector<uint64_t> seed(1);
		seq.generate(seed.begin(), seed.end());
		static_cast<base *>(this)->set_seed(seed[0]);
	};

	rng_base(uint64_t seed) {
		static_cast<base *>(this)->set_seed(seed);
	}

	std::string type() const {
		return typeid(base).name();
	}

	void checkpoint_write(const iodump::group &d) {
		d.write("seed", seed_);
		d.write("type", type());
		static_cast<base *>(this)->backend_checkpoint_write(d);
	}

	void checkpoint_read(const iodump::group &d) {
		d.read("seed", seed_);
		std::string read_type;
		d.read("type", read_type);
		if(read_type != type()) {
			throw std::runtime_error(
			    fmt::format("dump file was created using rng backend '{}' while this version of "
			                "loadleveller was built with backend '{}'",
			                read_type, type()));
		}
		static_cast<base *>(this)->backend_checkpoint_read(d);
	}

	// implemented by base:
	//

	uint32_t rand_integer(uint32_t bound); // in [0,bound)
	double rand_double();                  // in [0,1]
};

// based on a dinosaur code in the MersenneTwister.h header
class rng_internal_mersenne {
private:
	MTRand mtrand_;
public:
	void backend_checkpoint_write(const iodump::group &dump_file);
	void backend_checkpoint_read(const iodump::group &dump_file);
	void set_seed(uint64_t seed);

	double random_double() {
		return mtrand_.randDblExc(1);
	}
	
	int random_integer(int bound) {
		return mtrand_.randInt(bound - 1);
	}
};

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

// RNG_BACKEND is a macro set by the build system. If you add backends and you can help it,
// avoid using huge blocks of #ifdefs as it will lead to dead code nobody uses for 10 years.
// using random_number_generator = rng_base<RNG_BACKEND, #RNG_BACKEND>;

using random_number_generator = rng_base<RNG_BACKEND>;
}
