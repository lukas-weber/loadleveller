#ifndef MCL_RANDOM_H
#define MCL_RANDOM_H

#include "dump.h"
#include "types.h"
#include <random>

class randomnumbergenerator
{
public:
	randomnumbergenerator();
	randomnumbergenerator(luint seed);
	~randomnumbergenerator() {}
	void write(odump&);
	void read(idump&);
	double d() {return uniform(gen);}                 //in (0,1)
	luint seed() {return _seed;}

	std::mt19937 gen;
private:
	luint _seed;
	std::uniform_real_distribution<double> uniform;

};


#endif

