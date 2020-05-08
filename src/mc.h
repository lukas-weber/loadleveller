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
	size_t sweep_{0};
	size_t therm_{0};
	int pt_sweeps_per_global_update_{-1};

protected:
	parser param;
	std::unique_ptr<random_number_generator> rng;

	virtual void init() = 0;
	virtual void checkpoint_write(const iodump::group &out) = 0;
	virtual void checkpoint_read(const iodump::group &in) = 0;
	virtual void do_update() = 0;
	virtual void do_measurement() = 0;
	virtual void pt_update_param(const std::string & /*param_name*/, double /*new_param*/) {
		throw std::runtime_error{"running parallel tempering, but pt_update_param not implemented"};
	}
	virtual double pt_weight_ratio(const std::string & /*param_name*/, double /*new_param*/) {
		throw std::runtime_error{"running parallel tempering, but pt_weight_ratio not implemented"};
		return 1;
	}

	double random01() {
		return rng->random_double();
	}

public:
	bool pt_mode_{};

	size_t sweep() const;

	// implement this static function in your class!
	//static void register_evalables(evaluator &evalables);

	virtual void write_output(const std::string &filename);

	// these functions do a little more, like taking care of the
	// random number generator state, then call the child class versions.
	void _init();

	void _write(const std::string &dir);
	void _write_finalize(const std::string &dir);
	bool _read(const std::string &dir);

	void _do_update();
	void _do_measurement();
	void _pt_update_param(int target_rank, const std::string &param_name, double new_param);
	double _pt_weight_ratio(const std::string &param_name, double new_param);

	bool is_thermalized();
	measurements measure;

	mc(const parser &p);
	virtual ~mc() = default;
};

typedef std::function<mc *(const parser &)> mc_factory;
}
