#include "measurements.h"
#include <fmt/format.h>
#include <mpi.h>
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

void measurements::register_observable(const std::string &name, size_t bin_size,
                                       size_t vector_length) {
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
		register_observable(obs_name);
		observables_.at(obs_name).checkpoint_read(obs_name, dump_file.open_group(obs_name));
	}
}

void measurements::samples_write(const iodump::group &meas_file) {
	for(auto &obs : observables_) {
		auto g = meas_file.open_group(obs.first);
		obs.second.measurement_write(g);
	}
}

void measurements::mpi_sendrecv(int target_rank) {
	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	if(rank == target_rank) {
		return;
	}

	if(mpi_checked_targets_.count(target_rank) == 0) {
		if(rank < target_rank) {
			unsigned long obscount = observables_.size();
			MPI_Send(&obscount, 1, MPI_UNSIGNED_LONG, target_rank, 0, MPI_COMM_WORLD);
			for(auto &[name, obs] : observables_) {
				(void)obs;
				int size = name.size() + 1;
				MPI_Send(&size, 1, MPI_INT, target_rank, 0, MPI_COMM_WORLD);
				MPI_Send(name.c_str(), size, MPI_CHAR, target_rank, 0, MPI_COMM_WORLD);
			}
		} else {
			unsigned long obscount;
			MPI_Recv(&obscount, 1, MPI_UNSIGNED_LONG, target_rank, 0, MPI_COMM_WORLD,
			         MPI_STATUS_IGNORE);
			if(obscount != observables_.size()) {
				throw std::runtime_error{fmt::format(
				    "ranks {}&{} have to contain identical sets of registered observables. But "
				    "they contain different amounts of observables! {} != {}.",
				    target_rank, rank, obscount, observables_.size())};
			}

			for(auto &[name, obs] : observables_) {
				(void)obs;
				int size;
				MPI_Recv(&size, 1, MPI_INT, target_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				std::vector<char> buf(size);
				MPI_Recv(buf.data(), size, MPI_CHAR, target_rank, 0, MPI_COMM_WORLD,
				         MPI_STATUS_IGNORE);
				if(std::string{buf.data()} != name) {
					throw std::runtime_error{
					    fmt::format("ranks {}&{} have to contain identical sets of registered "
					                "observables. Found '{}' != '{}'.",
					                target_rank, rank, name, std::string{buf.data()})};
				}
			}
		}
		mpi_checked_targets_.insert(target_rank);
	}

	for(auto &[name, obs] : observables_) {
		(void)name;
		obs.mpi_sendrecv(target_rank);
	}
}

}
