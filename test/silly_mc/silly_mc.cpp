#include "silly_mc.h"
#include <valarray>

silly_mc::silly_mc(const loadl::parser &p) : loadl::mc(p) {}

void silly_mc::do_update() {
	idx_++;
}

void silly_mc::do_measurement() {
	std::vector<double> silly = {1. * idx_, 1. * (3 - idx_ % 5) * idx_};
	measure.add("MagicNumber", silly);
	measure.add("MagicNumber2", idx_ * idx_);
	std::valarray<double> silly2 = {1. * idx_, 1. * (3 - idx_ % 5) * idx_};
	measure.add("MagicNumberValarray", silly2);
}

void silly_mc::init() {
	idx_ = 0;

	measure.register_observable("MagicNumberValarray", 10);
}

void silly_mc::checkpoint_write(const loadl::iodump::group &d) {
	d.write("idx", idx_);
}

void silly_mc::checkpoint_read(const loadl::iodump::group &d) {
	d.read("idx", idx_);
}

void silly_mc::register_evalables(loadl::evaluator &eval) {
	eval.evaluate("AntimagicNumber", {"MagicNumber", "MagicNumber2"},
	              [](const std::vector<std::vector<double>> &obs) {
		              double mag = obs[0][0];
		              double mag2 = obs[1][0];

		              return std::vector{mag * mag / mag2};
	              });
}
