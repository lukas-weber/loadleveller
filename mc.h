#pragma once

#include <memory>
#include <vector>
#include <string>
#include "measurements.h"
#include "random.h"
#include "parser.h"

class abstract_mc {
private:
        void random_write(iodump& dump_file);
        void seed_write(const std::string& fn);
        void random_read(iodump& dump_file);
        void random_init();

	int sweep_ = 0;
	int therm_ = 0;
protected:
	parser param;
        std::unique_ptr<randomnumbergenerator> rng;
	
	virtual void init() = 0;
	virtual void checkpoint_write(iodump& out) = 0;
	virtual void checkpoint_read(iodump& in) = 0;
	virtual void write_output(const std::string& filename) = 0;
	virtual void do_update() = 0;
public:	
        double random01();
	int sweep() const;

	virtual void do_measurement() = 0;

	// these functions do a little more, like taking care of the
	// random number generator state, then call the child class versions.
	void _init();
	void _write(const std::string& dir);
	bool _read(const std::string& dir);

	void _write_output(const std::string& filename);

	void _do_update();
	
	bool is_thermalized();
	measurements measure;	
	
	abstract_mc(const std::string& taskfile);
	virtual ~abstract_mc();
};
