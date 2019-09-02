#include "results.h"
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>

namespace loadl {

void results::write_json(const std::string &filename, const std::string &taskdir,
                         const nlohmann::json &params) {
	using json = nlohmann::json;

	json obs_list;
	
	for(auto &[obs_name, obs] : observables) {
		double max_auto_time = 0;
		if(obs.autocorrelation_time.size() > 0) {
			max_auto_time =
			    *std::max_element(obs.autocorrelation_time.begin(), obs.autocorrelation_time.end());
		}

		obs_list[obs_name] = {
			{"rebin_len", obs.rebinning_bin_length},
			{"rebin_count", obs.rebinning_bin_count},
			{"internal_bin_len", obs.internal_bin_length},
			{"autocorr_time", max_auto_time},
			{"mean", obs.mean},
			{"error", obs.error},
		};
		
	}
	
	nlohmann::json out = {
		{"task", taskdir},
		{"parameters", params},
		{"results", obs_list}
	};

	std::ofstream file(filename);
	file << out.dump(1);
}
}
