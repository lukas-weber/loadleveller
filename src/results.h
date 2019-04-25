#pragma once
#include <map>
#include <vector>

namespace YAML {
class Node;
}

namespace loadl {

struct observable_result {
	std::string name;

	size_t rebinning_bin_length = 0;
	size_t rebinning_bin_count = 0;
	std::vector<double> rebinning_means; // [bin * vector_length + vector_idx]

	size_t total_sample_count = 0;

	// This is the bin length that was used when measuring the
	// samples. If different runs had different internal_bin_lengths,
	// this value is undefined.
	//
	// But it’s just a convenience feature, so that’s not bad.
	size_t internal_bin_length = 0;

	std::vector<double> mean;
	std::vector<double> error;

	std::vector<double> autocorrelation_time;
};


// results holds the means and errors merged from all the runs belonging to a task
// this includes both regular observables and evalables.
struct results {
	std::map<std::string, observable_result> observables;

	// writes out the results in a yaml file.
	void write_yaml(const std::string &filename, const std::string &taskdir,
	                const YAML::Node &params);
};
}
