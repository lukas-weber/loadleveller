#pragma once

#include <functional>
#include <map>

#include "evalable.h"
#include "results.h"

class merger {
public:	
	results merge(const std::vector<std::string>& filenames, size_t rebinning_bin_count = 32);

	// call this before merge
	void add_evalable(const std::string& name, const std::vector<std::string>& used_observables, evalable::func func);
private:
	std::vector<evalable> evalables_;
	void evaluate_evalables(results &res);
};

/*
template <class MCImpl>
int merge(int argc, char *argv[]);*/
