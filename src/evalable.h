#pragma once

#include "results.h"
#include <functional>
#include <string>
#include <vector>

namespace loadl {

class evaluator {
public:
	// Internally all observables are vectors, so you need a function
	//
	// std::vector<double> calculate_stuff(const std::vector<std::vector<double>>& obs);
	//
	// to create your evalable. The vector then contains all the observable values you
	// specified when constructing the evalable
	//
	typedef std::function<std::vector<double>(const std::vector<std::vector<double>> &observables)>
	    func;

	evaluator(results &res);
	void evaluate(const std::string &name, const std::vector<std::string> &used_observables,
	              func fun);

	// appends the evalable results to the other observables in res
	void append_results();

private:
	std::vector<observable_result> evalable_results_;
	results &res_;

	observable_result jackknife(const std::string &name,
	                            const std::vector<std::string> &used_observables, func fun) const;
};

}
