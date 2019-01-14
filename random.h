#pragma once

#include "dump.h"

#define MCL_RNG_MT

// everything but MCL_RNG_MT is old and needs to be ported to the new api if you want to use it.
// in that case, also get rid of the macros and use templates to switch between different generators or so.

#ifdef MCL_RNG_MT
#include "MersenneTwister.h"
class randomnumbergenerator
{
public:
	randomnumbergenerator();
	randomnumbergenerator(uint64_t seed);
	void checkpoint_write(iodump& dump_file);
	void checkpoint_read(iodump& dump_file);
	uint64_t seed() {return seed_;}
	double d(double supp=1) {return mtrand_.randDblExc(supp);}  //in (0,supp)
	int i(int bound) {return mtrand_.randInt(bound-1);}       //in [0,bound)

	double norm(); // normal distribution, mean = 0, std = 1

private:
	MTRand mtrand_;
	uint64_t seed_;
};
#endif

#ifdef MCL_RNG_SPRNG_4
#include "sprng_cpp.h"
class randomnumbergenerator
{
public:
	randomnumbergenerator();
	randomnumbergenerator(uint64_t seed);
	~randomnumbergenerator() {delete ptr;}
	void write(odump&);
	void read(idump&);
	uint64_t seed() {return my_seed;}
	double d() {return ptr->sprng();}         //in (0,1)
	double d(double supp) {return supp*d();}  //in (0,supp)
	int i(int bound) {return int(bound*d());} //in [0,bound)

private:
	Sprng * ptr;
	uint64_t my_seed;
};
#endif


#ifdef MCL_RNG_BOOST
#include <boost/random.hpp>
class randomnumbergenerator
{
public:
	randomnumbergenerator();
	randomnumbergenerator(int seed);
	void write(odump&);
	void read(idump&);
	uint64_t seed() {return my_seed;}
	double d() {return (*val)(*rng);}    	  //returns a value between 0 (excl) and 1 (excl).
	double d(double supp) {return supp*d();}  //returns a value between 0 (excl) and supp (excl.)
	int i(int bound) {return int(bound*d());} //returns a value between 0 (incl) and bound (excl.)

private:
	boost::mt19937 * rng;
	boost::uniform_real<> * val;
	uint64_t my_seed;
};
#endif


