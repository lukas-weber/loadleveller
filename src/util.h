#pragma once
#include <vector>

namespace loadl {
// mathematical utility functions
//

class monotonic_interpolator {
private:
	std::vector<double> x_, y_, m_;

public:
	// x and y are sorted
	monotonic_interpolator(const std::vector<double> &x, const std::vector<double> &y);

	double operator()(double x0);
};

}
