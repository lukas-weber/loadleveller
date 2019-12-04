#include "evalable.h"
#include "measurements.h"
#include <fmt/format.h>
#include <iostream>
#include <map>

namespace loadl {

evaluator::evaluator(results &res) : res_{res} {}

void evaluator::evaluate(const std::string &name, const std::vector<std::string> &used_observables,
                         func fun) {
	// evalable names also should be valid HDF5 paths
	if(not measurements::observable_name_is_legal(name)) {
		throw std::runtime_error{
		    fmt::format("illegal evalable name '{}': must not contain . or /", name)};
	}

	observable_result o = jackknife(name, used_observables, fun);

	// don’t include empty results
	if(o.rebinning_bin_count > 0) {
		evalable_results_.emplace_back(o);
	}
}

void evaluator::append_results() {
	for(auto &eval : evalable_results_) {
		res_.observables.emplace(eval.name, eval);
	}
}

observable_result evaluator::jackknife(const std::string &name,
                                       const std::vector<std::string> &used_observables,
                                       func fun) const {
	observable_result obs_res;

	std::vector<observable_result> observables;
	observables.reserve(used_observables.size());

	obs_res.name = name;

	size_t bin_count = -1; // maximal value
	for(const auto &obs_name : used_observables) {
		if(res_.observables.count(obs_name) <= 0) {
			std::cerr << fmt::format(
			    "Warning: evalable '{}': used observable '{}' not found in Monte Carlo results. "
			    "Skipping...\n",
			    name, obs_name);
			return obs_res;
		}
		const auto &obs = res_.observables.at(obs_name);

		if(obs.rebinning_bin_count < bin_count) {
			bin_count = obs.rebinning_bin_count;
		}

		observables.emplace_back(obs);
	}

	if(bin_count == 0) {
		return obs_res;
	}
	obs_res.rebinning_bin_count = bin_count;

	std::vector<std::vector<double>> jacked_means(observables.size());
	std::vector<std::vector<double>> sums(observables.size());

	for(size_t obs_idx = 0; obs_idx < observables.size(); obs_idx++) {
		size_t vector_length = observables[obs_idx].mean.size();
		sums[obs_idx].resize(vector_length, 0);
		jacked_means[obs_idx].resize(vector_length);
		for(size_t i = 0; i < vector_length; i++) {
			for(size_t k = 0; k < bin_count; k++) {
				sums[obs_idx][i] += observables[obs_idx].rebinning_means[k * vector_length + i];
			}
		}
	}

	// calculate the function estimator from the jacked datasets
	std::vector<double> jacked_eval_mean;
	for(size_t k = 0; k < bin_count; k++) {
		for(size_t obs_idx = 0; obs_idx < observables.size(); obs_idx++) {
			size_t vector_length = observables[obs_idx].mean.size();
			jacked_means[obs_idx].resize(vector_length);

			for(size_t i = 0; i < vector_length; i++) {
				jacked_means[obs_idx][i] =
				    (sums[obs_idx][i] -
				     observables[obs_idx].rebinning_means[k * vector_length + i]) /
				    (bin_count - 1);
			}
		}

		std::vector<double> jacked_eval = fun(jacked_means);
		if(jacked_eval_mean.empty())
			jacked_eval_mean.resize(jacked_eval.size(), 0);
		if(jacked_eval_mean.size() != jacked_eval.size()) {
			throw std::runtime_error(
			    fmt::format("evalable '{}': evalables must not change their dimensions depending "
			                "on the input",
			                name));
		}

		for(size_t i = 0; i < jacked_eval.size(); i++) {
			jacked_eval_mean[i] += jacked_eval[i];
		}
	}

	for(size_t i = 0; i < jacked_eval_mean.size(); i++) {
		jacked_eval_mean[i] /= bin_count;
	}

	// calculate the function estimator from the complete dataset.
	for(size_t obs_idx = 0; obs_idx < observables.size(); obs_idx++) {
		jacked_means[obs_idx] = sums[obs_idx];
		for(auto &jacked_mean : jacked_means[obs_idx]) {
			jacked_mean /= bin_count;
		}
	}

	std::vector<double> complete_eval = fun(jacked_means);
	assert(complete_eval.size() == jacked_eval_mean.size());

	// calculate bias-corrected jackknife estimator
	obs_res.mean.resize(jacked_eval_mean.size());
	for(size_t i = 0; i < obs_res.mean.size(); i++) {
		obs_res.mean[i] = bin_count * complete_eval[i] - (bin_count - 1) * jacked_eval_mean[i];
	}

	// now for the error
	for(size_t k = 0; k < bin_count; k++) {
		for(size_t obs_idx = 0; obs_idx < observables.size(); obs_idx++) {
			size_t vector_length = observables[obs_idx].mean.size();
			jacked_means[obs_idx].resize(vector_length);

			for(size_t i = 0; i < vector_length; i++) {
				jacked_means[obs_idx][i] =
				    (sums[obs_idx][i] -
				     observables[obs_idx].rebinning_means[k * vector_length + i]) /
				    (bin_count - 1);
			}
		}

		std::vector<double> jacked_eval = fun(jacked_means);
		if(obs_res.error.empty()) {
			obs_res.error.resize(jacked_eval.size(), 0);
		}

		for(size_t i = 0; i < obs_res.error.size(); i++) {
			obs_res.error[i] += pow(jacked_eval[i] - jacked_eval_mean[i], 2);
		}
	}
	for(size_t i = 0; i < obs_res.error.size(); i++) {
		obs_res.error[i] = sqrt((bin_count - 1) * obs_res.error[i] / bin_count);
	}

	return obs_res;
}
}
