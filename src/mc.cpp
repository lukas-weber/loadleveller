#include "mc.h"
namespace loadl {

mc::mc(const parser &p) : param{p} {
	therm_ = param.get<int>("thermalization");
}

void mc::write_output(const std::string &) {}

void mc::random_init() {
	if(param.defined("seed")) {
		rng.reset(new random_number_generator(param.get<uint64_t>("seed")));
	} else {
		rng.reset(new random_number_generator());
	}
}

double mc::random01() {
	return rng->random_double();
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
	
	double measurement_time = (tend.tv_sec - tstart.tv_sec) + 1e-9 * (tend.tv_nsec - tstart.tv_nsec);
	measure.add("_ll_measurement_time", measurement_time);
	if(measurement_time > max_meas_time_) {
		max_meas_time_ = measurement_time;
	}
}

void mc::_do_update() {
	struct timespec tstart, tend;
	clock_gettime(CLOCK_MONOTONIC_RAW, &tstart);
	sweep_++;

	do_update();
	clock_gettime(CLOCK_MONOTONIC_RAW, &tend);

	double sweep_time = (tend.tv_sec - tstart.tv_sec) + 1e-9 * (tend.tv_nsec - tstart.tv_nsec);
	measure.add("_ll_sweep_time", sweep_time);
	if(sweep_time > max_sweep_time_) {
		max_sweep_time_ = sweep_time;
	}
}

void mc::_pt_update_param(double new_param, const std::string &new_dir) {
	// take over the bins of the new target dir
	{
		iodump dump_file = iodump::create(new_dir + ".dump.h5.tmp");
		measure.checkpoint_read(dump_file.get_root().open_group("measurements"));
	}
	pt_update_param(new_param);
}

void mc::_write(const std::string &dir) {
	struct timespec tstart, tend;
	clock_gettime(CLOCK_MONOTONIC_RAW, &tstart);

	// blocks limit scopes of the dump file handles to ensure they are closed at the right time.
	{
		iodump meas_file = iodump::open_readwrite(dir + ".meas.h5");
		measure.samples_write(meas_file.get_root());
	}

	{
		iodump dump_file = iodump::create(dir + ".dump.h5.tmp");
		auto g = dump_file.get_root();

		measure.checkpoint_write(g.open_group("measurements"));
		rng->checkpoint_write(g.open_group("random_number_generator"));
		checkpoint_write(g.open_group("simulation"));

		g.write("max_checkpoint_write_time", max_checkpoint_write_time_);
		g.write("max_sweep_time", max_sweep_time_);
		g.write("max_meas_time", max_meas_time_);

		g.write("sweeps", sweep_);
		g.write("thermalization_sweeps", std::min(therm_, sweep_)); // only for convenience
	}
	rename((dir + ".dump.h5.tmp").c_str(), (dir + ".dump.h5").c_str());
	

	clock_gettime(CLOCK_MONOTONIC_RAW, &tend);
	double checkpoint_write_time = (tend.tv_sec - tstart.tv_sec) + 1e-9 * (tend.tv_nsec - tstart.tv_nsec);
	measure.add("_ll_checkpoint_write_time", checkpoint_write_time);
	if(checkpoint_write_time > max_checkpoint_write_time_) {
		max_checkpoint_write_time_ = checkpoint_write_time;
	}
}

double mc::safe_exit_interval() {
	// this is more or less guesswork in an attempt to make it safe for as many cases as possible
	return 2*(max_checkpoint_write_time_ + max_sweep_time_ + max_meas_time_) + 2;
}


bool mc::_read(const std::string &dir) {
	if(!file_exists(dir + ".dump.h5")) {
		return false;
	}

	struct timespec tstart, tend;
	clock_gettime(CLOCK_MONOTONIC_RAW, &tstart);

	iodump dump_file = iodump::open_readonly(dir + ".dump.h5");
	auto g = dump_file.get_root();

	rng.reset(new random_number_generator());
	rng->checkpoint_read(g.open_group("random_number_generator"));
	measure.checkpoint_read(g.open_group("measurements"));
	checkpoint_read(g.open_group("simulation"));

	g.read("sweeps", sweep_);

	g.read("max_checkpoint_write_time", max_checkpoint_write_time_);
	g.read("max_sweep_time", max_sweep_time_);
	g.read("max_meas_time", max_meas_time_);

	clock_gettime(CLOCK_MONOTONIC_RAW, &tend);
	measure.add("_ll_checkpoint_read_time",
	            (tend.tv_sec - tstart.tv_sec) + 1e-9 * (tend.tv_nsec - tstart.tv_nsec));
	return true;
}

void mc::_write_output(const std::string &filename) {
	write_output(filename);
}

bool mc::is_thermalized() {
	return sweep_ > therm_;
}
}
