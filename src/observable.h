#pragma once

#include "iodump.h"
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace loadl {

class observable {
public:
	observable(std::string name, size_t bin_length, size_t vector_length);

	const std::string &name() const;

	template<class T, std::enable_if_t<std::is_arithmetic_v<std::remove_reference_t<T>>> * = nullptr>
	void add(T val);

	template<class T>
	auto add(const T& val) -> decltype(val[0], void());

	void checkpoint_write(const iodump::group &dump_file) const;

	// This will empty the cache of already completed bins
	void measurement_write(const iodump::group &meas_file);

	void checkpoint_read(const std::string &name, const iodump::group &dump_file);

	// switch copy with target rank.
	// useful for parallel tempering mode
	void mpi_sendrecv(int target_rank);
private:
	static const size_t initial_bin_length = 1000;

	std::string name_;
	size_t bin_length_{};
	size_t vector_length_{};
	size_t current_bin_{};
	size_t current_bin_filling_{};

	std::vector<double> samples_;
};

template<class T, std::enable_if_t<std::is_arithmetic_v<std::remove_reference_t<T>>> * = nullptr>
void observable::add(T val) {
	add(std::array<T,1>{val});
}


template<class T>
auto observable::add(const T &val) -> decltype(val[0], void()) {
	if(vector_length_ != val.size()) {
		throw std::runtime_error{
		    fmt::format("observable::add: added vector has unexpected size ({}). Observable was "
		                "initialized with vector length ({})",
		                val.size(), vector_length_)};
	}

	for(size_t j = 0; j < vector_length_; ++j)
		samples_[j + current_bin_ * vector_length_] += static_cast<double>(val[j]);
	current_bin_filling_++;

	if(current_bin_filling_ == bin_length_) { // need to start a new bin next time
		if(bin_length_ > 1) {
			for(size_t j = 0; j < vector_length_; ++j) {
				samples_[current_bin_ * vector_length_ + j] /= bin_length_;
			}
		}
		current_bin_++;
		samples_.resize((current_bin_ + 1) * vector_length_);
		current_bin_filling_ = 0;
	}
}
}
