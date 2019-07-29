#include "mc.h"
namespace loadl {

mc::mc(const parser &p) : param{p} {
	therm_ = p.get<int>("thermalization");
	pt_sweeps_per_global_update_ = p.get<int>("pt_sweeps_per_global_update", -1);
}

void mc::write_output(const std::string &) {}

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

	if(pt_mode_) {
		if(param.get<bool>("pt_statistics", false)) {
			measure.add_observable("_ll_pt_rank", 1);
		}
	}

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

void mc::_pt_update_param(int target_rank, const std::string& param_name, double new_param) {
	measure.mpi_sendrecv(target_rank);
	pt_update_param(param_name, new_param);
}

void mc::pt_measure_statistics() {
	if(param.get<bool>("pt_statistics", false)) {
		int rank;
		MPI_Comm_rank(MPI_COMM_WORLD, &rank);
		measure.add("_ll_pt_rank", rank);
	}
}

double mc::_pt_weight_ratio(const std::string& param_name, double new_param) {
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

		g.write("max_checkpoint_write_time", max_checkpoint_write_time_);
		g.write("max_sweep_time", max_sweep_time_);
		g.write("max_meas_time", max_meas_time_);

		g.write("sweeps", sweep_);
		g.write("thermalization_sweeps", std::min(therm_, sweep_)); // only for convenience
	}
	rename((dir + ".dump.h5.tmp").c_str(), (dir + ".dump.h5").c_str());

	clock_gettime(CLOCK_MONOTONIC_RAW, &tend);
	double checkpoint_write_time =
	    (tend.tv_sec - tstart.tv_sec) + 1e-9 * (tend.tv_nsec - tstart.tv_nsec);
	measure.add("_ll_checkpoint_write_time", checkpoint_write_time);
	if(checkpoint_write_time > max_checkpoint_write_time_) {
		max_checkpoint_write_time_ = checkpoint_write_time;
	}
}

double mc::safe_exit_interval() {
	// this is more or less guesswork in an attempt to make it safe for as many cases as possible
	return 2 * (max_checkpoint_write_time_ + max_sweep_time_ + max_meas_time_) + 2;
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
	int sweep = sweep_;
	if(pt_mode_ && pt_sweeps_per_global_update_ > 0) {
		sweep /= pt_sweeps_per_global_update_;
	}

	return sweep >= therm_;
}
}
