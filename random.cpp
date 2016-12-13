#include "random.h"

#ifdef MCL_RNG_MT
randomnumbergenerator :: randomnumbergenerator()
{
	ptr = new MTRand();
	my_seed=ptr->get_seed();
}

randomnumbergenerator :: randomnumbergenerator(luint seed)
{
	ptr = new MTRand(seed);
	my_seed=ptr->get_seed();
}

void randomnumbergenerator :: write(odump& d)
{
	MTRand::uint32* randState;
	randState = new MTRand::uint32[ MTRand::SAVE ];
	ptr->save( randState );
	d.write(randState,MTRand::SAVE);
	d.write(my_seed);
	delete [] randState;
}

void randomnumbergenerator :: read(idump& d)
{
	MTRand::uint32* randState;
	randState = new MTRand::uint32[ MTRand::SAVE ];
	d.read(randState,MTRand::SAVE);
	d.read(my_seed);
	ptr->load( randState );
	delete [] randState;
}
#endif

#ifdef  MCL_RNG_SPRNG_4
randomnumbergenerator :: randomnumbergenerator()
{
	int gtype = 4; //Multipl. Lagg. Fib. 
	ptr = SelectType(gtype);
	my_seed=make_sprng_seed();
	ptr->init_sprng(0, 1, my_seed, SPRNG_DEFAULT);
}

randomnumbergenerator :: randomnumbergenerator(luint seed)
{
	int gtype = 4; //Multipl. Lagg. Fib.
	ptr = SelectType(gtype);
	my_seed=seed;
	ptr->init_sprng(0, 1, my_seed, SPRNG_DEFAULT);
}

void randomnumbergenerator :: write(odump& d)
{
	char* buffer;
	int buffersize=ptr->pack_sprng(&buffer);
	d.write(buffersize);
	d.write(buffer,buffersize);
	d.write(my_seed);
	delete buffer;
}

void randomnumbergenerator :: read(idump& d)
{
	char* buffer;
	int buffersize;
	d.read(buffersize);
	buffer = new char[buffersize];
	d.read(buffer,buffersize);
	d.read(my_seed);
	ptr->unpack_sprng(buffer);
	delete buffer;
}
#endif

#ifdef MCL_RNG_BOOST
void randomnumbergenerator :: write(odump& d)
{
}

void randomnumbergenerator :: read(idump& d)
{
}

randomnumbergenerator :: randomnumbergenerator()
{
	rng = new boost::mt19937;
	val = new boost::uniform_real<> (0.0, 1.0);
	
	my_seed = 5489;
}

randomnumbergenerator :: randomnumbergenerator(int seed)
{
	rng = new boost::mt19937;
	rng->seed(seed);
	val = new boost::uniform_real<> (0.0, 1.0);
	
	my_seed = seed;
}
#endif
// 
// #ifdef MCL_RNG_ACML
// randomnumbergenerator :: randomnumbergenerator(int seed)
// {
// 	gen = new acml_rand(seed, 0., 1.);
// }
//
// #endif

