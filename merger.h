#pragma once

#include <functional>
#include <map>

#include "evalable.h"

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

struct merger_results {
	// contains both evalables and pure observables 
	std::map<std::string, observable_result> observables;
};

class merger {
public:	
	merger_results merge(const std::vector<std::string>& filenames, size_t rebinning_bin_count = 32);

	// call this before merge
	void add_evalable(const std::string& name, const std::vector<std::string>& used_observables, evalable::func func);
private:
	std::vector<evalable> evalables_;
	void evaluate_evalables(merger_results &res);
};

/*
template <class MCImpl>
int merge(int argc, char *argv[]);*/
