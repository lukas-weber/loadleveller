#include "result.h"
#include <yaml-cpp/yaml.h>

void results::write_yaml(const std::string& filename, const std::string& taskname, const YAML::Node& config) {
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << taskname;
	out << YAML::Value << YAML::BeginMap;
	for(auto& [obs_name, obs] : observables) {
		out << YAML::Key << obs_name << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "rebinning_bin_length" << YAML::Value << obs.rebinning_bin_length;
		out << YAML::Key << "rebinning_bin_count" << YAML::Value << obs.rebinning_bin_count;
		out << YAML::Key << "internal_bin_count" << YAML::Value << obs.internal_bin_count;
		out << YAML::Key << "autocorrelation_time" << YAML::Value << std::max_element(obs.autocorrelation_time.begin(), obs.autocorrelation_time.end());
		out << YAML::Key << "mean" << YAML::Value << obs.mean;
		out << YAML::Key << "error" << YAML::Value << obs.error;
		out << YAML::EndMap;
	}
	out << YAML::EndMap;
	out << YAML::EndMap;
}
