#include "observable.h"
#include <fmt/format.h>
#include <iostream>
namespace loadl {

observable::observable(std::string name, size_t bin_length, size_t vector_length)
    : name_{std::move(name)}, bin_length_{bin_length}, vector_length_{vector_length} {
	samples_.reserve(vector_length_ * initial_bin_length);
	samples_.resize(vector_length_);
}

const std::string &observable::name() const {
	return name_;
}

void observable::checkpoint_write(const iodump::group &dump_file) const {
	// The plan is that before checkpointing, all complete bins are written to the measurement file.
	// Then only the incomplete bin remains and we write that into the dump to resume
	// the filling process next time.
	// So if current_bin_ is not 0 here, we have made a mistake.
	assert(current_bin_ == 0);

	// Another sanity check: the samples_ array should contain one partial bin.
	assert(samples_.size() == vector_length_);

	dump_file.write("name", name_);
	dump_file.write("vector_length", vector_length_);
	dump_file.write("bin_length", bin_length_);
	dump_file.write("current_bin_filling", current_bin_filling_);
	dump_file.write("samples", samples_);
}

void observable::measurement_write(const iodump::group &meas_file) {
	if(samples_.size() > vector_length_) {
		std::vector<double> current_bin_value(samples_.end() - vector_length_, samples_.end());

		samples_.resize(current_bin_ * vector_length_);
		meas_file.insert_back("samples", samples_);
		samples_ = current_bin_value;
		assert(samples_.size() == vector_length_);
	} else {
		meas_file.insert_back("samples", std::vector<double>()); // just touch
	}

	meas_file.write("vector_length", vector_length_);
	meas_file.write("bin_length", bin_length_);

	current_bin_ = 0;
}

void observable::checkpoint_read(const iodump::group &d) {
	d.read("name", name_);
	d.read("vector_length", vector_length_);
	d.read("bin_length", bin_length_);
	d.read("current_bin_filling", current_bin_filling_);
	d.read("samples", samples_);
	current_bin_ = 0;
}

bool observable::is_clean() const {
	if(current_bin_filling_ != 0) {
		std::cout << current_bin_filling_ << "\n";
	}
	return current_bin_ == 0 && current_bin_filling_ == 0;
}

}
