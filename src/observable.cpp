#include "observable.h"
#include <mpi.h>
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

observable observable::checkpoint_read(const std::string &name, const iodump::group &d) {
	size_t vector_length, bin_length;
	d.read("vector_length", vector_length);
	d.read("bin_length", bin_length);
	
	observable obs{name, bin_length, vector_length};
	d.read("current_bin_filling", obs.current_bin_filling_);
	d.read("samples", obs.samples_);
	return obs;
}

void observable::mpi_sendrecv(int target_rank) {
	const int msg_size = 4;
	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	unsigned long msg[msg_size] = {current_bin_, vector_length_, bin_length_, current_bin_filling_};
	MPI_Sendrecv_replace(msg, msg_size, MPI_UNSIGNED_LONG, target_rank, 0, target_rank, 0,
	                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	current_bin_ = msg[0];
	vector_length_ = msg[1];
	bin_length_ = msg[2];
	current_bin_filling_ = msg[3];

	std::vector<double> recvbuf((current_bin_ + 1) * vector_length_);
	MPI_Sendrecv(samples_.data(), samples_.size(), MPI_DOUBLE, target_rank, 0, recvbuf.data(),
	             recvbuf.size(), MPI_DOUBLE, target_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

	samples_ = recvbuf;
}

}
