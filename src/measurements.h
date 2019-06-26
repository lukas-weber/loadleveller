#pragma once

#include "dump.h"
#include "observable.h"
#include <map>
#include <string>
#include <valarray>
#include <vector>

namespace loadl {

class measurements {
public:
	static bool observable_name_is_legal(const std::string &name);

	void add_observable(const std::string &name, size_t bin_size = 1, size_t vector_length = 1);

	// use this to add a measurement sample to an observable.
	template<class T>
	void add(const std::string name, T value);

	void checkpoint_write(const iodump::group &dump_file);
	void checkpoint_read(const iodump::group &dump_file);

	// samples_write needs to be called before checkpoint_write and the meas_file
	// should be opened in read/write mode.
	void samples_write(const iodump::group &meas_file);

private:
	std::map<std::string, observable> observables_;
};

template<class T>
void measurements::add(const std::string name, T value) {
	if(observables_.count(name) == 0) {
		throw std::runtime_error{fmt::format("tried to add to observable '{}' which was not registered!", name)};
	}
	observables_.at(name).add(value);
}
}
