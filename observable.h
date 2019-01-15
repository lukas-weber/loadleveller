#ifndef MCL_OBSERVABLE_H
#define MCL_OBSERVABLE_H

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



#endif

