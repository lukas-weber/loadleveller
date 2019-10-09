#include "mc.h"
namespace loadl {

mc::mc(const parser &p) : param{p} {
	therm_ = p.get<int>("thermalization");
	pt_sweeps_per_global_update_ = p.get<int>("pt_sweeps_per_global_update", 1);
}

void mc::write_output(const std::string &) {}

int mc::sweep() const {
	return sweep_;
}

void mc::_init() {
	// simple profiling support: measure the time spent for sweeps/measurements etc
	measure.register_observable("_ll_checkpoint_read_time", 1);
	measure.register_observable("_ll_checkpoint_write_time", 1);
	measure.register_observable("_ll_measurement_time", 1000);
	measure.register_observable("_ll_sweep_time", 1000);

	if(param.defined("seed")) {
		rng.reset(new random_number_generator(param.get<uint64_t>("seed")));
	} else {
		rng.reset(new random_number_generator());
	}

	init();
}

void mc::_do_measurement() {
	struct timespec tstart, tend;
	clock_gettime(CLOCK_MONOTONIC_RAW, &tstart);

	do_measurement();

	clock_gettime(CLOCK_MONOTONIC_RAW, &tend);

	double measurement_time =
	    (tend.tv_sec - tstart.tv_sec) + 1e-9 * (tend.tv_nsec - tstart.tv_nsec);
	measure.add("_ll_measurement_time", measurement_time);
}

void mc::_do_update() {
	struct timespec tstart, tend;
	clock_gettime(CLOCK_MONOTONIC_RAW, &tstart);
	sweep_++;

	do_update();
	clock_gettime(CLOCK_MONOTONIC_RAW, &tend);

	double sweep_time = (tend.tv_sec - tstart.tv_sec) + 1e-9 * (tend.tv_nsec - tstart.tv_nsec);
	measure.add("_ll_sweep_time", sweep_time);
}

void mc::_pt_update_param(int target_rank, const std::string &param_name, double new_param) {
	measure.mpi_sendrecv(target_rank);
	pt_update_param(param_name, new_param);
}

double mc::_pt_weight_ratio(const std::string &param_name, double new_param) {
	double wr = pt_weight_ratio(param_name, new_param);
	return wr;
}

void mc::_write(const std::string &dir) {
	struct timespec tstart, tend;
	clock_gettime(CLOCK_MONOTONIC_RAW, &tstart);

	// blocks limit scopes of the dump file handles to ensure they are closed at the right time.
	{
		iodump meas_file = iodump::open_readwrite(dir + ".meas.h5");
		auto g = meas_file.get_root();
		measure.samples_write(g);
	}

	{
		iodump dump_file = iodump::create(dir + ".dump.h5.tmp");
		auto g = dump_file.get_root();

		rng->checkpoint_write(g.open_group("random_number_generator"));
		checkpoint_write(g.open_group("simulation"));
		measure.checkpoint_write(g.open_group("measurements"));

		int therm = therm_;
		if(pt_mode_) {
			therm *= pt_sweeps_per_global_update_;
		}
		g.write("thermalization_sweeps", std::min(sweep_,therm));
		g.write("sweeps", std::max(0,sweep_-therm));
	}

	clock_gettime(CLOCK_MONOTONIC_RAW, &tend);
	double checkpoint_write_time =
	    (tend.tv_sec - tstart.tv_sec) + 1e-9 * (tend.tv_nsec - tstart.tv_nsec);
	measure.add("_ll_checkpoint_write_time", checkpoint_write_time);
}

// This function is called if it is certain that the *.tmp files have been completely written.
// Important for parallel tempering mode where all slaves in a chain have to write consistent dumps.
void mc::_write_finalize(const std::string &dir) {
	rename((dir + ".dump.h5.tmp").c_str(), (dir + ".dump.h5").c_str());
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

	int sweeps, therm_sweeps;
	g.read("thermalization_sweeps", therm_sweeps);
	g.read("sweeps", sweeps);
	sweep_ = sweeps + therm_sweeps;

	clock_gettime(CLOCK_MONOTONIC_RAW, &tend);
	measure.add("_ll_checkpoint_read_time",
	            (tend.tv_sec - tstart.tv_sec) + 1e-9 * (tend.tv_nsec - tstart.tv_nsec));
	return true;
}

bool mc::is_thermalized() {
	int sweep = sweep_;
	if(pt_mode_ && pt_sweeps_per_global_update_ > 0) {
		sweep /= pt_sweeps_per_global_update_;
	}

	return sweep >= therm_;
}
}
