#include <random>
#include <iostream>

#include <loadleveller/random/mt19937.h>

int main(int, char **) {
	uint32_t seed = 1234;

	loadl::mt19937 rng;
	std::mt19937 stl_rng;

	rng.seed(seed);
	stl_rng.seed(seed);

	for(int i = 0; i < 100; i++) {
		uint32_t r0 = rng.rng();
		uint32_t r1 = stl_rng();

		std::cout << r0 << " = " << r1 << "\n";
		if(r0 != r1) {
			return 1;
		}
	}
	return 0;	
}
