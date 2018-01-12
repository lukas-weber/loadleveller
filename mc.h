#pragma once

#include <iostream>
#include <vector>
#include <string>
#include "measurements.h"
#include "random.h"
#include "parser.h"
#include "types.h"

using namespace std;

class abstract_mc
{
	private:

                void param_init(string dir);
                void random_clear();
                void random_write(odump& d);
                void seed_write(string fn);
                void random_read(idump& d);
		void random_init();

		int _sweep = 0;
		int therm = 0;
	protected:
		parser param;
                randomnumbergenerator *rng = 0;
                double random01();
		
		virtual void init() = 0;
		virtual void write(odump &out) = 0;
		virtual bool read(idump &in) = 0;
		virtual void write_output(std::ofstream &f) = 0;
		virtual void do_update() = 0;
	public:	
		int sweep() const;

		virtual void do_measurement() = 0;

		// these functions do a little more, like taking care of the
		// random number generator state, then call the child class versions.
		void _init();
		void _write(std::string dir);
		bool _read(std::string dir);
		void _write_output(std::string filename);

		void _do_update();
		
		bool is_thermalized();
		measurements measure;	
		
		abstract_mc(string dir);
		virtual ~abstract_mc();
};
