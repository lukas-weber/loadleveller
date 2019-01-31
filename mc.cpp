#include "mc.h"

abstract_mc::abstract_mc(const std::string& jobfile, const std::string& taskname) 
	: param{parser{jobfile}["tasks"][taskname]} {
	
	therm_ = param.get<int>("thermalization");
}

abstract_mc::~abstract_mc() {
}

void abstract_mc::random_init() {
	if(param.defined("seed"))
		rng.reset(new randomnumbergenerator(param.get<uint64_t>("seed")));
	else
		rng.reset(new randomnumbergenerator());
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
	measure.samples_write(meas_file.get_root());

	iodump dump_file = iodump::create(dir+".dump.h5");
	auto g = dump_file.get_root();
	
	measure.checkpoint_write(g.open_group("measurements"));
	rng->checkpoint_write(g.open_group("random_number_generator"));
	checkpoint_write(g.open_group("simulation"));

	g.write("sweeps", sweep_);
	g.write("thermalization_sweeps", std::min(therm_,sweep_)); // only for convenience
}

bool abstract_mc::_read(const std::string& dir) {
	try {
		iodump dump_file = iodump::open_readonly(dir+".dump.h5");
		auto g = dump_file.get_root();
		measure.checkpoint_read(g.open_group("measurements"));
		rng->checkpoint_read(g.open_group("random_number_generator"));
		checkpoint_read(g.open_group("simulation"));

		g.read("sweeps", sweep_);
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

