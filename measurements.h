#pragma once
#ifndef MCL_MEASUREMENTS_APPEND
#define MCL_MEASUREMENTS_APPEND 0
#endif

#include <string>
#include <vector>
#include <map>
#include <valarray>
#include "dump.h"
#include "observable.h"

class measurements {
public:
	static bool observable_name_is_legal(const std::string& name);

	void add_observable(const std::string& name, size_t bin_size = 1, size_t vector_length_ = 1, size_t initial_length = 1000);

	// use this to add a measurement sample to an observable.
	template <class T> 
	void add(const std::string name, T value);

	void checkpoint_write(iodump& dump_file);
	void checkpoint_read(iodump& dump_file);
	
	// samples_write needs to be called before checkpoint_write and the meas_file
	// should be opened in read/write mode.
	void samples_write(iodump& meas_file);
private:
	std::map<std::string, observable> observables_;
};

template <class T> 
void measurements::add(const std::string name, T value) {
	observables_.at(name).add(value);
}
