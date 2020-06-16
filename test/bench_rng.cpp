#include <iostream>
#include <loadleveller/random.h>

int main(int, char **) {
	const uint64_t nsamples = 200000000;

	loadl::random_number_generator rng(123456789);

	double sum = 0;
	for(uint64_t i = 0; i < nsamples; i++) {
		sum += rng.random_double();
		sum += rng.random_double();
		sum += rng.random_double();
		sum += rng.random_double();
		sum += rng.random_double();
		sum += rng.random_double();
		sum += rng.random_double();
		sum += rng.random_double();
		sum += rng.random_double();
		sum += rng.random_double();
		sum += rng.random_double();
		sum += rng.random_double();
		sum += rng.random_double();
		sum += rng.random_double();
		sum += rng.random_double();
		sum += rng.random_double();
	}

	std::cout << sum / (16 * nsamples);
}
