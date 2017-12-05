#include "mc.h"

abstract_mc :: abstract_mc (string dir) {
	param_init(dir);

	therm=param.value_or_default<int>("THERMALIZATION",10000);
}

abstract_mc :: ~abstract_mc() {
	random_clear();
}

void abstract_mc::random_init() {
	if (param.defined("SEED"))
		rng = new randomnumbergenerator(param.value_of<luint>("SEED"));
	else
		rng = new randomnumbergenerator();
}
void abstract_mc::param_init(string dir) {
	param.read_file(dir);
}

void abstract_mc::random_write(odump& d) {
	rng->write(d);
}

void abstract_mc::seed_write(string fn) {
	ofstream s;
	s.open(fn.c_str());
	s << rng->seed()<<endl;s.close();
}

void abstract_mc::random_read(idump& d) {
	rng = new randomnumbergenerator();
	rng->read(d);
}

void abstract_mc::random_clear() { 
	delete rng;
	rng = 0;
}

double abstract_mc::random01() {
	return rng->d();
}

int abstract_mc::sweep() const {
	return _sweep;
}
		
void abstract_mc::_init() {
	random_init();
	init();
}

void abstract_mc::_do_update() {
	_sweep++;
	do_update();
}

void abstract_mc::_write(std::string dir) {
	odump d(dir+"dump");
	random_write(d);
	d.write(_sweep);
	write(d);
	d.close();
	seed_write(dir+"seed");
	dir+="sweeps";
	ofstream f; f.open(dir.c_str());
	f << ( (is_thermalized()) ? _sweep-therm : 0 ) << " " << _sweep <<endl;
	f.close();
}

bool abstract_mc::_read(std::string dir) {
	idump d(dir+"dump");
	if (!d) 
		return false;
	random_read(d);
	d.read(_sweep);
	if(!read(d))
		return false;

	d.close();
	return true;
}

void abstract_mc::_write_output(std::string filename) {
	ofstream f(filename.c_str());
	write_output(f);
	f << "PARAMETERS" << endl;
	param.get_all(f);
	measure.get_statistics(f);
	f.close();
}

bool abstract_mc::is_thermalized() {
	return _sweep>therm;
}

