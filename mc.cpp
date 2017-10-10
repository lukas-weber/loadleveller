#include "mc.h"

mc :: mc (string dir) {
	param_init(dir);
}

mc :: ~mc() {
	random_clear();
}

void mc::random_init() {
	if (param.defined("SEED"))
		rng = new randomnumbergenerator(param.value_of<luint>("SEED"));
	else
		rng = new randomnumbergenerator();
	have_random=true;
}
void mc::param_init(string dir) {
	param.read_file(dir);
}

void mc::random_write(odump& d) {
	rng->write(d);
}

void mc::seed_write(string fn) {
	ofstream s;
	s.open(fn.c_str());
	s << rng->seed()<<endl;s.close();
}

void mc::random_read(idump& d) {
	rng = new randomnumbergenerator();
	have_random=true;
	rng->read(d);
}

void mc::random_clear() { 
	if(have_random) {
		delete rng;
		have_random=false;
	}
}

double mc::random01() {
	return rng->d();
}
