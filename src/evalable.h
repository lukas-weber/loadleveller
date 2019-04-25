#pragma once

#include "results.h"
#include <functional>
#include <string>
#include <vector>

namespace loadl {

class evalable {
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

	evalable(std::string name, std::vector<std::string> used_observables, func fun);
	const std::string &name() const;
	void jackknife(const results &res, observable_result &out) const;

private:
	const std::string name_;
	const std::vector<std::string> used_observables_;
	const func fun_;
};
}
