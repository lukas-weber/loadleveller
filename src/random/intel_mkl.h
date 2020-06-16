#pragma once

#include "iodump.h"
#include <mkl_vsl.h>
#include <vector>

namespace loadl {

class rng_intel_mkl {
private:
	static const int buffer_size_ = 10000;

	std::array<double, buffer_size_> buffer_;
	int buffer_idx_{buffer_size_};

	VSLStreamStatePtr random_stream_{};

	void refill_buffer() {
		vdRngUniform(VSL_RNG_METHOD_UNIFORM_STD, random_stream_, buffer_size_, buffer_.data(), 0,
		             1);
	}

public:
	void backend_checkpoint_write(const iodump::group &dump_file) {
		std::vector<char> save_buf(vslGetStreamSize(random_stream_));
		int rc = vslSaveStreamM(random_stream_, save_buf.data());
		if(rc != VSL_ERROR_OK) {
			throw std::runtime_error{
			    fmt::format("Could not write Intel MKL RNG: error code {}", rc)};
		}
		dump_file.write("state", save_buf);
		dump_file.write("buffer", std::vector<double>(buffer_.begin(), buffer_.end()));
		dump_file.write("buffer_idx", buffer_idx_);
	}

	void backend_checkpoint_read(const iodump::group &dump_file) {
		if(random_stream_ != nullptr) {
			vslDeleteStream(&random_stream_);
		}
		std::vector<char> load_buf;
		dump_file.read("state", load_buf);
		int rc = vslLoadStreamM(&random_stream_, load_buf.data());
		if(rc != VSL_ERROR_OK) {
			throw std::runtime_error{
			    fmt::format("Could not load Intel MKL RNG: error code {}", rc)};
		}

		std::vector<double> tmpbuffer;
		dump_file.read("buffer", tmpbuffer);
		assert(tmpbuffer.size() == buffer_.size());
		std::copy(tmpbuffer.begin(), tmpbuffer.end(), buffer_.begin());
		dump_file.read("buffer_idx", buffer_idx_);
	}

	void set_seed(uint64_t seed) {
		if(random_stream_ != nullptr) {
			vslDeleteStream(&random_stream_);
		}
		int rc = vslNewStream(&random_stream_, VSL_BRNG_SFMT19937, seed);
		if(rc != VSL_ERROR_OK) {
			throw std::runtime_error{
			    fmt::format("Could not initialize Intel MKL RNG: error code {}", rc)};
		}
	}

	double random_double() {
		if(buffer_idx_ >= buffer_size_) {
			refill_buffer();
			buffer_idx_ = 0;
		}
		return buffer_[buffer_idx_++];
	}

	uint32_t random_integer(uint32_t bound) {
		return bound * random_double(); // TODO: use the optimized integer distribution at the cost
		                                // of a second buffer?
	}

	~rng_intel_mkl() {
		vslDeleteStream(&random_stream_);
	}
};

}
