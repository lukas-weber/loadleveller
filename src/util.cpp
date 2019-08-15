#include "util.h"
#include <cassert>
#include <cmath>

namespace loadl {

//https://en.wikipedia.org/wiki/Monotone_cubic_interpolation
monotonic_interpolator::monotonic_interpolator(const std::vector<double>& x, const std::vector<double>& y) : x_(x), y_(y), m_(x.size()) {
	assert(x.size() == y.size());
	assert(x.size() > 1);

	std::vector<double> d(x.size());
	for(size_t i = 0; i < x.size()-1; i++) {
		d[i] = (y[i+1]-y[i])/(x[i+1]-x[i]);
	}
	m_[0] = d[0];
	m_[x.size()-1] = d[x.size()-2];
	for(size_t i = 1; i < x.size()-1; i++) {
		m_[i] = (d[i-1]+d[i])/2;

		if(d[i-1]*d[i] <= 0) {
			m_[i] = 0;
		}
	}
	for(size_t i = 0; i < x.size()-1; i++) {
		double a = m_[i]/d[i];
		double b = m_[i+1]/d[i];

		double r = a*a + b*b;
		if(r > 9) {
			m_[i] *= 3/sqrt(r);
			m_[i+1] *= 3/sqrt(r);
		}
	}
}

static double h00(double t) {
	return (1+2*t)*(1-t)*(1-t);
}

static double h10(double t) {
	return t*(1-t)*(1-t);
}

static double h01(double t) {
	return t*t*(3-2*t);
}

static double h11(double t) {
	return t*t*(t-1);
}

double monotonic_interpolator::operator()(double x0) {
	// replace by binary search if necessary!
	size_t idx = 0;
	assert(x0 < x_[x_.size()-1]);
	while(x0 >= x_[idx]) {
		idx++;
	}
	assert(idx >= 1);
	idx--;
	assert(idx < x_.size()-1);

	double del = x_[idx+1]-x_[idx];
	double t = (x0-x_[idx])/del;

	return y_[idx]*h00(t)+del*m_[idx]*h10(t)+y_[idx+1]*h01(t)+del*m_[idx+1]*h11(t);
}
}
