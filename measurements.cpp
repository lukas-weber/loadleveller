#include "measurements.h"
#include <fmt/format.h>

bool measurements::observable_name_is_legal(const std::string& obs_name) {
	if(obs_name.find('/') != obs_name.npos)
		return false;
	if(obs_name.find('.') != obs_name.npos)
		return false;
	return true;
}

void measurements::add_observable(const std::string& name, size_t bin_size, size_t vector_length, size_t initial_length) {
	if(not observable_name_is_legal(name)) {
		throw std::runtime_error(fmt::format("Illegal observable name '{}': names must not contain / or .", name));
	}

	observables_.emplace(name, observable{name, bin_size, vector_length, initial_length});
}

template <class T> 
void measurements::add(const std::string name, T value) {
	observables_.at(name).add(value);
}


void measurements::checkpoint_write(iodump& dump_file) {
	dump_file.change_group("measurements");

	for(const auto& obs : observables_) {
		dump_file.change_group(obs.first);
		obs.second.checkpoint_write(dump_file);
		dump_file.change_group("..");
	}

	dump_file.change_group("..");
}

void measurements::checkpoint_read(iodump& dump_file) {
	dump_file.change_group("measurements");

	for(const auto& obs_name : dump_file.list()) {
		dump_file.change_group(obs_name);
		add_observable(obs_name);
		observables_.at(obs_name).checkpoint_read(dump_file);
		dump_file.change_group("..");
	}

	dump_file.change_group("..");
}

void measurements::samples_write(iodump& meas_file) {
	for(auto& obs : observables_) {
		meas_file.change_group(obs.first);
		obs.second.measurement_write(meas_file);
		meas_file.change_group("..");
	}
}

