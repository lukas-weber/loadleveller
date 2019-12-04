#pragma once

#include "iodump.h"
#include "observable.h"
#include <map>
#include <set>
#include <string>
#include <valarray>
#include <vector>

namespace loadl {

class measurements {
public:
	measurements(size_t default_bin_size);

	static bool observable_name_is_legal(const std::string &name);

	// only needed if you want to set the bin_size explicitly
	// otherwise observables are automatically created on first add
	void register_observable(const std::string &name, size_t bin_size);

	// use this to add a measurement sample to an observable.
	template<class T>
	void add(const std::string name, T value);

	void checkpoint_write(const iodump::group &dump_file);
	void checkpoint_read(const iodump::group &dump_file);

	// samples_write needs to be called before checkpoint_write and the meas_file
	// should be opened in read/write mode.
	void samples_write(const iodump::group &meas_file);

	// switches the content of the measurement buffers with the target_rank
	// both ranks must have the same set of observables!
	void mpi_sendrecv(int target_rank);

private:
	std::set<int> mpi_checked_targets_;
	std::map<std::string, observable> observables_;

	const size_t default_bin_size_{1};

	template<class T>
	size_t value_length(const T &val) {
		if constexpr(std::is_arithmetic_v<T>) {
			return 1;
		} else {
			return val.size();
		}
	}
};

template<class T>
void measurements::add(const std::string name, T value) {
	if(observables_.count(name) == 0) {
		observables_.emplace(name, observable{name, default_bin_size_, value_length(value)});
	}
	observables_.at(name).add(value);
}
}
