#include <catch2/catch.hpp>
#include "evalable.h"
#include "results.h"
#include "random.h"
#include <fmt/format.h>

using namespace loadl;

TEST_CASE("square of uniform distribution") {
	random_number_generator rng{1337};

	int ntries = 10000;
	size_t nsamples = GENERATE(100, 1000);

	std::vector<double> tries(ntries);
	double jackknife_error{};

	for(int t = 0; t < ntries; t++) {
		results res;
		for(int var = 0; var < 2; var++) {
			std::vector<double> samples(nsamples);
			double sum = 0;
			double squared_sum = 0;
			for(auto &s : samples) {
				s = rng.random_double();
				sum += s;
				squared_sum += s*s;
			}

			double mean = sum/nsamples;
			double error = sqrt((squared_sum-nsamples*mean*mean)/(nsamples-1));

			std::string name = fmt::format("Uniform{}", var);
			res.observables[name] = observable_result{name, 1, nsamples, samples, nsamples, 0, {mean}, {error}, {0.}};
		}

		evaluator eval{res};
		eval.evaluate("UniformSquared", {"Uniform0", "Uniform1"}, [](const std::vector<std::vector<double>> &obs) {
			return std::vector<double>{obs[0][0]*obs[1][0]};
		});

		eval.append_results();

		tries[t] = res.observables["UniformSquared"].mean[0];
		jackknife_error = res.observables["UniformSquared"].error[0];
	}

	double sum_of_tries = 0;
	double sum_of_tries_squared = 0;

	for(const auto &r : tries) {
		sum_of_tries += r;
		sum_of_tries_squared += r*r;
	}

	double tries_mean = sum_of_tries/ntries;
	double tries_std = sqrt((sum_of_tries_squared - ntries*tries_mean*tries_mean)/(ntries-1));

	REQUIRE(tries_mean == Approx(0.25).epsilon(0.05));
	REQUIRE(tries_std == Approx(jackknife_error).epsilon(0.05));

}
