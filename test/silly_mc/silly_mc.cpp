#include "silly_mc.h"

silly_mc::silly_mc(const loadl::parser &p) : loadl::mc(p) {
}

void silly_mc::do_update() {
	idx_++;
}

void silly_mc::do_measurement() {
	std::vector<double> silly = {1.*idx_, 1.*(3-idx_%5)*idx_};
	measure.add("MagicNumber", silly);
	measure.add("MagicNumber2", idx_*idx_);
}

void silly_mc::init() {
	idx_=0;

	int binsize = param.get("binsize", 1);
	measure.register_observable("MagicNumber", binsize,2);
	measure.register_observable("MagicNumber2", binsize);
}

void silly_mc::checkpoint_write(const loadl::iodump::group &d) {
	d.write("idx", idx_);
}

void silly_mc::checkpoint_read(const loadl::iodump::group &d) {
	d.read("idx", idx_);
}

void silly_mc::register_evalables(std::vector<loadl::evalable> &evalables) {
	evalables.push_back(
	    loadl::evalable{"AntimagicNumber",
	                    {"MagicNumber", "MagicNumber2"},
	                    [](const std::vector<std::vector<double>> &obs) -> std::vector<double> {
		                    double mag = obs[0][0];
		                    double mag2 = obs[1][0];

		                    return {mag * mag / mag2};
	                    }});
}
