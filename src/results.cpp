#include "results.h"
#include <algorithm>
#include <fstream>
#include <yaml-cpp/yaml.h>

namespace loadl {

void results::write_yaml(const std::string &filename, const std::string &taskdir,
                         const YAML::Node &params) {
	YAML::Emitter out;
	out << YAML::BeginSeq;
	out << YAML::BeginMap;
	out << YAML::Key << "task" << YAML::Value << taskdir;
	out << YAML::Key << "parameters" << YAML::Value << params;
	out << YAML::Key << "results" << YAML::Value << YAML::BeginMap;
	for(auto & [obs_name, obs] : observables) {
		out << YAML::Key << obs_name;
		if(obs.internal_bin_length == 0) {
			out << YAML::Comment("evalable");
		}
		out << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "rebinning_bin_length" << YAML::Value << obs.rebinning_bin_length;
		out << YAML::Key << "rebinning_bin_count" << YAML::Value << obs.rebinning_bin_count;
		out << YAML::Key << "internal_bin_length" << YAML::Value << obs.internal_bin_length;
		double max_auto_time = 0;
		if(obs.autocorrelation_time.size() > 0) {
			max_auto_time =
			    *std::max_element(obs.autocorrelation_time.begin(), obs.autocorrelation_time.end());
		}
		out << YAML::Key << "autocorrelation_time" << YAML::Value << max_auto_time;
		out << YAML::Key << "mean" << YAML::Value << obs.mean;
		out << YAML::Key << "error" << YAML::Value << obs.error;
		out << YAML::EndMap;
	}
	out << YAML::EndMap;
	out << YAML::EndMap;
	out << YAML::EndSeq;

	std::ofstream file(filename);
	file << out.c_str();
}
}
