#pragma once

#include "evalable.h"
#include "measurements.h"
#include "parser.h"
#include "random.h"
#include <memory>
#include <string>
#include <vector>

namespace loadl {

class mc {
private:
	void random_init();

	int sweep_ = 0;
	int therm_ = 0;

	// The following times in seconds are used to estimate a safe exit interval before walltime is up.
	double max_checkpoint_write_time_{0};
	double max_sweep_time_{0};
	double max_meas_time_{0};
protected:
	parser param;
	std::unique_ptr<random_number_generator> rng;

	virtual void init() = 0;
	virtual void checkpoint_write(const iodump::group &out) = 0;
	virtual void checkpoint_read(const iodump::group &in) = 0;
	virtual void write_output(const std::string &filename);
	virtual void do_update() = 0;
	virtual void do_measurement() = 0;
	virtual void pt_update_param(double /*new_param*/) {
		throw std::runtime_error{"running parallel tempering, but pt_update_param not implemented"};
	}
public:
	double random01();
	int sweep() const;

	virtual void register_evalables(std::vector<evalable> &evalables) = 0;
	virtual double pt_weight_ratio(double /*new_param*/) {
		throw std::runtime_error{"running parallel tempering, but pt_weight_ratio not implemented"};
		return 1;
	}
	
	// these functions do a little more, like taking care of the
	// random number generator state, then call the child class versions.
	void _init();
	void _write(const std::string &dir);
	bool _read(const std::string &dir);

	void _write_output(const std::string &filename);

	void _do_update();
	void _do_measurement();
	void _pt_update_param(double new_param, const std::string &new_dir);

	double safe_exit_interval();

	bool is_thermalized();
	measurements measure;

	mc(const parser &p);
	virtual ~mc() = default;
};

typedef std::function<mc *(const parser &)> mc_factory;
}
