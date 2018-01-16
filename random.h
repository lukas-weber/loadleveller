#ifndef MCL_RANDOM_H
#define MCL_RANDOM_H

#include "dump.h"
#include "types.h"

#define MCL_RNG_MT

#ifdef MCL_RNG_MT
#include "MersenneTwister.h"
class randomnumbergenerator
{
public:
	randomnumbergenerator();
	randomnumbergenerator(luint seed);
	~randomnumbergenerator() {delete ptr;}
	void write(odump&);
	void read(idump&);
	luint seed() {return my_seed;}
	double d() {return ptr->randDblExc();}                 //in (0,1)
	double d(double supp) {return ptr->randDblExc(supp);}  //in (0,supp)
	int i(int bound) {return ptr->randInt(bound-1);}       //in [0,bound)

	double norm(); // normal distribution, mean = 0, std = 1

private:
	MTRand* ptr; 
	luint my_seed;
};
#endif

#ifdef MCL_RNG_SPRNG_4
#include "sprng_cpp.h"
class randomnumbergenerator
{
public:
	randomnumbergenerator();
	randomnumbergenerator(luint seed);
	~randomnumbergenerator() {delete ptr;}
	void write(odump&);
	void read(idump&);
	luint seed() {return my_seed;}
	double d() {return ptr->sprng();}         //in (0,1)
	double d(double supp) {return supp*d();}  //in (0,supp)
	int i(int bound) {return int(bound*d());} //in [0,bound)

private:
	Sprng * ptr;
	luint my_seed;
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
	luint seed() {return my_seed;}
	double d() {return (*val)(*rng);}    	  //returns a value between 0 (excl) and 1 (excl).
	double d(double supp) {return supp*d();}  //returns a value between 0 (excl) and supp (excl.)
	int i(int bound) {return int(bound*d());} //returns a value between 0 (incl) and bound (excl.)

private:
	boost::mt19937 * rng;
	boost::uniform_real<> * val;
	luint my_seed;
};
#endif

// #ifdef MCL_RNG_ACML
// #warning RNG: ACML
// #include "acml_rand.h"
// class randomnumbergenerator
// {
// 	public:
// 		randomnumbergenerator(int seed);
// 		double d() {return (*gen)();}         //returns a value between 0 (excl) and 1 (excl).
// 		double d(double supp) {return supp*d();}  //returns a value between 0 (excl) and supp (excl.)
// 		int i(int bound) {return int(bound*d());} //returns a value between 0 (incl) and bound (excl.)
// 
// 	private:
// 		acml_rand * gen;
// 
// };
// 
// #endif


#endif

