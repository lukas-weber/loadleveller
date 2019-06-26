#include "merger.h"
#include "dump.h"
#include "evalable.h"
#include "mc.h"
#include "measurements.h"

#include <fmt/format.h>
#include <string>
#include <vector>

namespace loadl {

static void evaluate_evalables(results &res, const std::vector<evalable> &evalables) {
	std::vector<observable_result> evalable_results;
	for(auto &eval : evalables) {
		observable_result o;
		eval.jackknife(res, o);

		// don’t include empty results
		if(o.rebinning_bin_count > 0) {
			evalable_results.emplace_back(o);
		}
	}

	for(auto &eval : evalable_results) {
		res.observables.emplace(eval.name, eval);
	}
}

results merge(const std::vector<std::string> &filenames, const std::vector<evalable> &evalables,
              size_t rebinning_bin_count) {
	results res;

	// This thing reads the complete time series of an observable which will
	// probably make it the biggest memory user of load leveller. But since
	// it’s only one observable of one run at a time, it is maybe still okay.

	// If not, research a custom solution using fancy HDF5 virtual datasets or something.

	// In the first pass we gather the metadata to decide on the rebinning_bin_length.
	for(auto &filename : filenames) {
		iodump meas_file = iodump::open_readonly(filename);
		auto g = meas_file.get_root();
		for(const auto &obs_name : g) {
			size_t vector_length;
			std::vector<double> samples;

			auto obs_group = g.open_group(obs_name);
			int sample_size = obs_group.get_extent("samples");

			if(sample_size == 0) { // ignore empty observables
				continue;
			}

			if(res.observables.count(obs_name) == 0)
				res.observables.emplace(obs_name, observable_result());
			auto &obs = res.observables.at(obs_name);
			obs.name = obs_name;

			obs_group.read("bin_length", obs.internal_bin_length);
			obs_group.read("vector_length", vector_length);
			obs_group.read("samples", samples);

			if(sample_size % vector_length != 0) {
				throw std::runtime_error{
				    "merge: sample count is not an integer multiple of the vector length. Corrupt "
				    "file?"};
			}

			obs.total_sample_count += sample_size / vector_length;
			obs.mean.resize(vector_length);
			obs.error.resize(vector_length);
			obs.autocorrelation_time.resize(vector_length);
		}
	}

	struct obs_rebinning_metadata {
		size_t current_rebin = 0;
		size_t current_rebin_filling = 0;
		size_t sample_counter = 0;
	};

	std::map<std::string, obs_rebinning_metadata> metadata;

	for(auto &entry : res.observables) {
		auto &obs = entry.second;

		if(rebinning_bin_count == 0) {
			// no rebinning before this
			size_t min_bin_count = 10;
			if(obs.total_sample_count <= min_bin_count) {
				obs.rebinning_bin_count = obs.total_sample_count;
			} else {
				obs.rebinning_bin_count = min_bin_count + cbrt(obs.total_sample_count-min_bin_count);
			}
		} else {
			obs.rebinning_bin_count = rebinning_bin_count;
		}

		obs.rebinning_means.resize(obs.rebinning_bin_count * obs.mean.size());
		obs.rebinning_bin_length = obs.total_sample_count / obs.rebinning_bin_count;

		metadata.emplace(obs.name, obs_rebinning_metadata{});
	}

	for(auto &filename : filenames) {
		iodump meas_file = iodump::open_readonly(filename);
		auto g = meas_file.get_root();
		for(auto & [obs_name, obs] : res.observables) {
			std::vector<double> samples;
			obs.name = obs_name;

			g.read(fmt::format("{}/samples", obs_name), samples);

			// rebinning_bin_count*rebinning_bin_length may be smaller than
			// total_sample_count. In that case, we throw away the leftover samples.
			//
			size_t vector_length = obs.mean.size();
			for(size_t i = 0; metadata[obs_name].sample_counter <
			                      obs.rebinning_bin_count * obs.rebinning_bin_length &&
			                  i < samples.size();
			    i++) {
				size_t vector_idx = i % vector_length;

				obs.mean[vector_idx] += samples[i];
				metadata[obs_name].sample_counter++;
			}
		}
	}

	for(auto &entry : res.observables) {
		auto &obs = entry.second;
		assert(metadata[entry.first].sample_counter ==
		       obs.rebinning_bin_count * obs.rebinning_bin_length);

		for(auto &mean : obs.mean) {
			mean /= metadata[entry.first].sample_counter;
		}
	}

	// now handle the error and autocorrelation time which are calculated by rebinning.
	for(auto &filename : filenames) {
		iodump meas_file = iodump::open_readonly(filename);
		auto g = meas_file.get_root();
		for(auto & [obs_name, obs] : res.observables) {
			std::vector<double> samples;
			auto &obs_meta = metadata.at(obs_name);

			size_t vector_length = obs.mean.size();

			g.read(fmt::format("{}/samples", obs_name), samples);

			for(size_t i = 0;
			    obs_meta.current_rebin < obs.rebinning_bin_count && i < samples.size(); i++) {
				size_t vector_idx = i % vector_length;
				size_t rebin_idx = obs_meta.current_rebin * vector_length + vector_idx;

				obs.rebinning_means[rebin_idx] += samples[i];

				if(vector_idx == 0)
					obs_meta.current_rebin_filling++;

				// I used autocorrelation_time as a buffer here to hold the naive no-rebinning error
				// (sorry)
				obs.autocorrelation_time[vector_idx] +=
				    (samples[i] - obs.mean[vector_idx]) * (samples[i] - obs.mean[vector_idx]);

				if(obs_meta.current_rebin_filling >= obs.rebinning_bin_length) {
					obs.rebinning_means[rebin_idx] /= obs.rebinning_bin_length;

					double diff = obs.rebinning_means[rebin_idx] - obs.mean[vector_idx];
					obs.error[vector_idx] += diff * diff;

					if(vector_idx == vector_length - 1) {
						obs_meta.current_rebin++;
						obs_meta.current_rebin_filling = 0;
					}
				}
			}
		}
	}

	for(auto &entry : res.observables) {
		auto &obs = entry.second;
		assert(metadata[entry.first].current_rebin == obs.rebinning_bin_count);
		for(size_t i = 0; i < obs.error.size(); i++) {
			int used_samples = obs.rebinning_bin_count * obs.rebinning_bin_length;
			double no_rebinning_error =
			    sqrt(obs.autocorrelation_time[i] / (used_samples - 1) / used_samples);

			obs.error[i] =
			    sqrt(obs.error[i] / (obs.rebinning_bin_count - 1) / (obs.rebinning_bin_count));

			obs.autocorrelation_time[i] = 0.5 * pow(obs.error[i] / no_rebinning_error, 2);
		}
	}

	evaluate_evalables(res, evalables);

	return res;
}
}
