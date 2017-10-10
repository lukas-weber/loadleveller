#pragma once

#include <iostream>
#include <vector>
#include <string>
#include "measurements.h"
#include "random.h"
#include "parser.h"
#include "types.h"

using namespace std;

class mc
{
	private:
                bool have_random = false;

                void param_init(string dir);
                void random_clear();


	protected:
                void random_write(odump& d);
                void seed_write(string fn);
                void random_read(idump& d);
		parser param;
	public:	
                double random01();
		int sweep;
                randomnumbergenerator * rng;
                
		void random_init();

		virtual void init() = 0;
		virtual void do_update() = 0;
		virtual void do_measurement() = 0;
		virtual void write(string) = 0;
		virtual bool read(string) = 0;
		virtual void write_output(string) = 0;
		
		virtual bool is_thermalized() = 0;
		measurements measure;	
		
		mc(string dir);
		virtual ~mc();
};
