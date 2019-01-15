#pragma once

#include <cmath>
#include <string>
#include <vector>
#include <map>
#include "dump.h"

class observable
{
public:	
	observable(std::string name, size_t vector_length = 1, size_t bin_length = 1, size_t initial_length = 1000);

	const std::string& name() const;

	template <class T>
	void add(T);
	
	template <class T>
	void add(const std::vector<T>&);

	void checkpoint_write(iodump& dump_file) const;
	void measurement_write(iodump& meas_file);
	
	void checkpoint_read(iodump& dump_file);

private:
	std::string name_;
	size_t bin_length_;
	size_t vector_length_;
	size_t initial_length_;
	size_t current_bin_;
	size_t current_bin_filling_;

	std::vector<double> samples_;
};


template <class T>
void observable::add(T val) {
	std::vector<T> v = {val};
	add(v);
}

template <class T> 
void observable::add(const std::vector<T>& val) {
	// handle wrong vector length gracefully on first add
	if(current_bin_ == 0 and current_bin_filling_ == 0) {
		vector_length_=val.size();
		samples_.resize((current_bin_+1)*vector_length_,0.);
	} else if(vector_length_ != val.size()) {
		throw std::runtime_error{fmt::format("observable::add: added vector has different size ({}) than what was added before ({})", val.size(), vector_length_)};
	}
		
	for(size_t j=0; j < vector_length_; ++j) 
		samples_[j+current_bin_*vector_length_] += static_cast<double>(val[j]);
	current_bin_filling_++;
	
	if(current_bin_filling_ == bin_length_) { //need to start a new bin next time
		if (bin_length_>1)
			for(size_t j = 0; j < vector_length_; ++j)
				samples_[current_bin_*vector_length_+j] /= bin_length_;
		current_bin_++;
		samples_.resize((current_bin_+1)*vector_length_);
		current_bin_filling_=0;
	}
}
