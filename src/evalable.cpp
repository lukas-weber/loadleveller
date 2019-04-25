#include "evalable.h"
#include "measurements.h"
#include <fmt/format.h>
#include <map>

namespace loadl {

evalable::evalable(std::string name, std::vector<std::string> used_observables, func fun)
    : name_{std::move(name)}, used_observables_{std::move(used_observables)}, fun_{std::move(fun)} {
	// evalable names also should be valid HDF5 paths
	if(not measurements::observable_name_is_legal(name)) {
		throw std::runtime_error{
		    fmt::format("illegal evalable name '{}': must not contain . or /", name)};
	}
}

const std::string &evalable::name() const {
	return name_;
}

void evalable::jackknife(const results &res, observable_result &obs_res) const {
	std::vector<observable_result> observables;
	observables.reserve(used_observables_.size());

	obs_res.name = name_;

	size_t bin_count = -1; // maximal value
	for(const auto &obs_name : used_observables_) {
		if(res.observables.count(obs_name) <= 0) {
			throw std::runtime_error(
			    fmt::format("evalable '{}': used observable '{}' not found in Monte Carlo results.",
			                name_, obs_name));
		}
		const auto &obs = res.observables.at(obs_name);

		if(obs.rebinning_bin_count < bin_count) {
			bin_count = obs.rebinning_bin_count;
		}

		observables.emplace_back(obs);
	}
	obs_res.rebinning_bin_count = bin_count;

	if(bin_count == 0) {
		return;
	}

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

		std::vector<double> jacked_eval = fun_(jacked_means);
		if(jacked_eval_mean.empty())
			jacked_eval_mean.resize(jacked_eval.size(), 0);
		if(jacked_eval_mean.size() != jacked_eval.size()) {
			throw std::runtime_error(
			    fmt::format("evalable '{}': evalables must not change their dimensions depending "
			                "on the input"));
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

	std::vector<double> complete_eval = fun_(jacked_means);
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

		std::vector<double> jacked_eval = fun_(jacked_means);
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
}
}
