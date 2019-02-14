#ifndef MCL_ACML_RAND_H
#define MCL_ACML_RAND_H

extern "C" void dranduniform_(int *N, double *A, double *B, int *state, double *X, int *info);
extern "C" void dranddiscreteuniform_(int *N, int *A, int *B, int *state, int *X, int *info);
extern "C" void drandinitialize_(int *GENID, int *SUBID, int *SEED, int *LSEED, int *STATE,
                                 int *LSTATE, int *INFO);
#include <cstdlib>
class acml_rand {
public:
	acml_rand(uint seed, double A, double B, int len = 1000) {
		len_ = len;
		vec = new double[len_];
		A_ = A;
		B_ = B;
		// fill seed array
		seed_len = 624;
		int *seed_array = new int[seed_len];
		srand48(seed);
		for(uint i = 0; i < (uint)seed_len; ++i) {
			seed_array[i] = (int)lrand48();
		}
		// initialize Mersenne PRNG int genid = 3;
		int info;
		int ignored = 0;
		int genid = 3;
		state_len = 700;
		state = new int[state_len];
		drandinitialize_(&genid, &ignored, seed_array, &seed_len, state, &state_len, &info);
		generate();
	}
	~acml_rand() {
		delete[] vec;
		delete[] state;
	}
	inline const double &operator()(void) {
		if(pos < len_)
			return vec[pos++];
		generate();
		return vec[pos++];
	}

private:
	void generate() {
		pos = 0;
		int info;
		dranduniform_(&len_, &A_, &B_, state, vec, &info);
	}
	int pos;
	int len_;
	int state_len;
	int seed_len;
	double *vec;
	int *state;
	double A_, B_;
};

class acml_rand_discrete {
public:
	acml_rand_discrete(uint seed, int A, int B, int len = 1000) {
		len_ = len;
		vec = new int[len_];
		A_ = A;
		B_ = B;
		// fill seed array
		seed_len = 624;
		int *seed_array = new int[seed_len];
		srand48(seed);
		for(uint i = 0; i < (uint)seed_len; ++i) {
			seed_array[i] = (int)lrand48();
		}
		// initialize Mersenne PRNG int genid = 3;
		int info;
		int ignored = 0;
		int genid = 3;
		//@fixme why 700? not 633?
		state_len = 700;
		state = new int[state_len];
		drandinitialize_(&genid, &ignored, seed_array, &seed_len, state, &state_len, &info);
		generate();
	}
	~acml_rand_discrete() {
		delete[] vec;
		delete[] state;
	}
	inline const int &operator()(void) {
		if(pos < len_)
			return vec[pos++];
		generate();
		return vec[pos++];
	}

private:
	void generate() {
		pos = 0;
		int info;
		dranddiscreteuniform_(&len_, &A_, &B_, state, vec, &info);
	}
	int pos;
	int len_;
	int state_len;
	int seed_len;
	int *vec;
	int *state;
	int A_, B_;
};
#endif
