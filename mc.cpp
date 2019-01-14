#include "mc.h"

abstract_mc::abstract_mc(const std::string& taskfile) {
	param.read_file(taskfile);

	therm_=param.value_or_default<int>("THERMALIZATION",10000);
}

abstract_mc::~abstract_mc() {
}

void abstract_mc::random_init() {
	if (param.defined("SEED"))
		rng.reset(new randomnumbergenerator(param.value_of<uint64_t>("SEED")));
	else
		rng.reset(new randomnumbergenerator());
}

void abstract_mc::random_write(iodump& d) {
	rng->checkpoint_write(d);
}

void abstract_mc::random_read(iodump& d) {
	rng.reset(new randomnumbergenerator());
	rng->checkpoint_read(d);
}

double abstract_mc::random01() {
	return rng->d();
}

int abstract_mc::sweep() const {
	return sweep_;
}
		
void abstract_mc::_init() {
	random_init();
	init();
}

void abstract_mc::_do_update() {
	sweep_++;
	do_update();
}

void abstract_mc::_write(const std::string& dir) {
	iodump meas_file = iodump::open_readwrite(dir+".meas.h5");
	measure.samples_write(meas_file);
	
	iodump dump_file = iodump::create(dir+".dump.h5");
	measure.checkpoint_write(dump_file);
	random_write(dump_file);

	dump_file.write("sweeps", sweep_);
	
	dump_file.write("thermalization_sweeps", std::min(therm_,sweep_)); // only for convenience
	checkpoint_write(dump_file);
}

bool abstract_mc::_read(const std::string& dir) {
	try {
		iodump dump_file = iodump::open_readonly(dir+"dump.h5");
		measure.checkpoint_read(dump_file);
		random_read(dump_file);
		dump_file.read("sweeps", sweep_);

		checkpoint_read(dump_file);

	} catch(const iodump_exception& e) {
		return false;
	}
	return true;
}

void abstract_mc::_write_output(const std::string& filename) {
	write_output(filename);
}

bool abstract_mc::is_thermalized() {
	return sweep_>therm_;
}

