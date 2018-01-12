#include "random.h"
#include <mpi.h>
#include <climits>

static inline luint hash(time_t t, clock_t c){
	// Get a uint64 from t and c
	// Better than uint32(x) in case x is floating point in [0,1]
	// Based on code by Lawrence Kirby (fred@genesis.demon.co.uk)

	static uint64_t differ = 0;  // guarantee time-based seeds will change

#ifndef MCL_SINGLE
	int myrank;
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
	differ=(uint64_t)(myrank);
#endif

	uint64_t h1 = 0;
	unsigned char *p = (unsigned char *) &t;
	for( size_t i = 0; i < sizeof(t); ++i )
	{
		h1 *= UCHAR_MAX + 2U;
		h1 += p[i];
	}
	uint64_t h2 = 0;
	p = (unsigned char *) &c;
	for( size_t j = 0; j < sizeof(c); ++j )
	{
		h2 *= UCHAR_MAX + 2U;
		h2 += p[j];
	}
	return ( h1 + differ++ ) ^ h2;
}

randomnumbergenerator :: randomnumbergenerator() : uniform(0,1)
{
	_seed = hash( time(NULL), clock());
	gen.seed(_seed);
}

randomnumbergenerator :: randomnumbergenerator(luint seed) : uniform(0,1)
{
	_seed = seed;
	gen.seed(seed);
}

void randomnumbergenerator :: write(odump& d)
{
	d.binary_file << gen;
	d.binary_file << uniform;
}

void randomnumbergenerator :: read(idump& d)
{
	d.binary_file >> gen;
	d.binary_file >> uniform;
}

