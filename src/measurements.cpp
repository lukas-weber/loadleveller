#include "measurements.h"
#include <fmt/format.h>

namespace loadl {

bool measurements::observable_name_is_legal(const std::string &obs_name) {
	if(obs_name.find('/') != obs_name.npos) {
		return false;
	}
	if(obs_name.find('.') != obs_name.npos) {
		return false;
	}
	return true;
}

void measurements::add_observable(const std::string &name, size_t bin_size, size_t vector_length) {
	if(!observable_name_is_legal(name)) {
		throw std::runtime_error(
		    fmt::format("Illegal observable name '{}': names must not contain / or .", name));
	}

	observables_.emplace(name, observable{name, bin_size, vector_length});
}

void measurements::checkpoint_write(const iodump::group &dump_file) {
	for(const auto &obs : observables_) {
		obs.second.checkpoint_write(dump_file.open_group(obs.first));
	}
}

void measurements::checkpoint_read(const iodump::group &dump_file) {
	for(const auto &obs_name : dump_file) {
		add_observable(obs_name);
		observables_.at(obs_name).checkpoint_read(dump_file.open_group(obs_name));
	}
}

void measurements::samples_write(const iodump::group &meas_file) {
	for(auto &obs : observables_) {
		auto g = meas_file.open_group(obs.first);
		obs.second.measurement_write(g);
	}
}

std::optional<std::string> measurements::is_unclean() const {
	for(const auto &obs : observables_) {
		if(!obs.second.is_clean()) {
			return obs.first;
		}
	}
	return std::nullopt;
}
}
