#include "catch.hpp"
#include "util.h"

using namespace loadl;

double testfun1(double x) {
	return x+sin(x);
}

double testfun2(double x) {
	return -x*x-sin(x*x);
}

double testfun3(double x) {
	return x*x*x;
}

double interpolate(double (*f)(double), double xmax, int npoints, int ncheck) {
	double dx = xmax/(npoints-1);
	std::vector<double> x(npoints);
	std::vector<double> y(npoints);

	for(int i = 0; i < npoints; i++) {
		x[i] = dx*i;
		y[i] = f(x[i]);
	}

	monotonic_interpolator inter(x, y);

	double rms = 0;
	double dx_check = xmax/(ncheck+1);
	for(int i = 0; i < ncheck; i++) {
		double x = i*dx_check;
		double y = inter(x);
		rms += (f(x)-y)*(f(x)-y);
	}

	rms = sqrt(rms)/ncheck;
	return rms;
}

TEST_CASE("monotonic interpolator") {
	REQUIRE(interpolate(testfun1, 10, 30, 100) < 0.0001);
	REQUIRE(interpolate(testfun2, 10, 150, 200) < 0.001);
	REQUIRE(interpolate(testfun2, 3, 60, 100) < 0.0001);
	REQUIRE(interpolate(testfun3, 1, 10, 100) < 0.0005);
}
