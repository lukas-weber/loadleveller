#ifndef MC_PT_H
#define MC_PT_H

#include <iostream>
#include <vector>
#include <string>
#include "measurements.h"
#include "random.h"
#include "parser.h"
#include "types.h"

using namespace std;

class mc_pt
{
	private:
		int Lx;
		int Ly;
		vector<double> Tvec;
		double T;
		int therm;
		int sweep;
		int pt_spacing;
		int label;
		vector<int> spin;
		
	public:	
		parser param;
		void param_init(string dir) {param.read_file(dir);}

                bool have_random;
                randomnumbergenerator * rng;
                void random_init() {if (param.defined("SEED"))
                                     rng = new randomnumbergenerator(param.value_of<luint>("SEED"));
                                    else
                                     rng = new randomnumbergenerator();
                                    have_random=true;
                                   }
                void random_write(odump& d) {rng->write(d);}
                void seed_write(string fn) {ofstream s; s.open(fn.c_str()); s << rng->seed()<<endl;s.close();}
                void random_read(idump& d) {rng = new randomnumbergenerator();have_random=true;rng->read(d);}
                void random_clear() { if (have_random) {delete rng;have_random=false;} }
                double random01() {return rng->d();}
		
		void init();
		void do_update();
		void do_measurement();
		void write(string);
		bool read(string);
		void write_output(string,int);
		
		bool is_thermalized();

		bool request_global_update();
		void change_parameter(int);
		void change_to(int);
		double get_weight(int);
		int get_label();

		int myrep;

		vector<measurements> measure;	
		
		mc_pt(string);
		~mc_pt();
};

#endif
