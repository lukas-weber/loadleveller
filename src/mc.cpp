#include "mc.h"

namespace loadl {

mc::mc(const parser &p) : param{p} {
	therm_ = param.get<int>("thermalization");
}

void mc::write_output(const std::string &) {}

void mc::random_init() {
	if(param.defined("seed")) {
		rng.reset(new randomnumbergenerator(param.get<uint64_t>("seed")));
	} else {
		rng.reset(new randomnumbergenerator());
	}
}

double mc::random01() {
	return rng->d();
}

int mc::sweep() const {
	return sweep_;
}

void mc::_init() {
	// simple profiling support: measure the time spent for sweeps/measurements etc
	measure.add_observable("_ll_checkpoint_read_time", 1);
	measure.add_observable("_ll_checkpoint_write_time", 1);
	measure.add_observable("_ll_measurement_time", 1000);
	measure.add_observable("_ll_sweep_time", 1000);
	random_init();
	init();
}

void mc::_do_measurement() {
	struct timespec tstart, tend;
	clock_gettime(CLOCK_MONOTONIC_RAW, &tstart);

	do_measurement();

	clock_gettime(CLOCK_MONOTONIC_RAW, &tend);
	measure.add("_ll_measurement_time",
	            (tend.tv_sec - tstart.tv_sec) + 1e-9 * (tend.tv_nsec - tstart.tv_nsec));
}

void mc::_do_update() {
	struct timespec tstart, tend;
	clock_gettime(CLOCK_MONOTONIC_RAW, &tstart);
	sweep_++;

	do_update();
	clock_gettime(CLOCK_MONOTONIC_RAW, &tend);
	measure.add("_ll_sweep_time",
	            (tend.tv_sec - tstart.tv_sec) + 1e-9 * (tend.tv_nsec - tstart.tv_nsec));
}

void mc::_write(const std::string &dir) {
	struct timespec tstart, tend;
	clock_gettime(CLOCK_MONOTONIC_RAW, &tstart);

	iodump meas_file = iodump::open_readwrite(dir + ".meas.h5");
	measure.samples_write(meas_file.get_root());

	iodump dump_file = iodump::create(dir + ".dump.h5");
	auto g = dump_file.get_root();

	measure.checkpoint_write(g.open_group("measurements"));
	rng->checkpoint_write(g.open_group("random_number_generator"));
	checkpoint_write(g.open_group("simulation"));

	g.write("sweeps", sweep_);
	g.write("thermalization_sweeps", std::min(therm_, sweep_)); // only for convenience

	clock_gettime(CLOCK_MONOTONIC_RAW, &tend);
	measure.add("_ll_checkpoint_write_time",
	            (tend.tv_sec - tstart.tv_sec) + 1e-9 * (tend.tv_nsec - tstart.tv_nsec));
}

bool mc::_read(const std::string &dir) {
	try {
		struct timespec tstart, tend;
		clock_gettime(CLOCK_MONOTONIC_RAW, &tstart);

		iodump dump_file = iodump::open_readonly(dir + ".dump.h5");
		auto g = dump_file.get_root();
		measure.checkpoint_read(g.open_group("measurements"));
		rng->checkpoint_read(g.open_group("random_number_generator"));
		checkpoint_read(g.open_group("simulation"));

		g.read("sweeps", sweep_);

		clock_gettime(CLOCK_MONOTONIC_RAW, &tend);
		measure.add("_ll_checkpoint_read_time",
		            (tend.tv_sec - tstart.tv_sec) + 1e-9 * (tend.tv_nsec - tstart.tv_nsec));
	} catch(const iodump_exception &e) {
		return false;
	}
	return true;
}

void mc::_write_output(const std::string &filename) {
	write_output(filename);
}

bool mc::is_thermalized() {
	return sweep_ > therm_;
}
}
