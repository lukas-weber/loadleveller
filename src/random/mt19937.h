#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include <cassert>

// This is a very simple implementation of the MT19937 RNG
// based on the implementation of the original authors
// http://www.math.sci.hiroshima-u.ac.jp/m-mat/MT/MT2002/CODES/mt19937ar.c

namespace loadl {
class mt19937 {
private:
	// depends on this type being 32 bit at the moment.
	using mt_int = uint32_t;
	static constexpr mt_int w_ = 8 * sizeof(mt_int);
	static constexpr mt_int n_ = 624;
	static constexpr mt_int m_ = 397;
	static constexpr mt_int r_ = 31;
	static constexpr mt_int a_ = 0x9908b0df;

	static constexpr mt_int u_ = 11;
	static constexpr mt_int d_ = 0xffffffff;
	static constexpr mt_int s_ = 7;
	static constexpr mt_int b_ = 0x9d2c5680;
	static constexpr mt_int t_ = 15;
	static constexpr mt_int c_ = 0xefc60000;
	static constexpr mt_int l_ = 18;

	static constexpr mt_int f_ = 1812433253;

	static constexpr mt_int lower_mask_ = (1U << r_) - 1U;
	static constexpr mt_int upper_mask_ = ~lower_mask_;

	static constexpr double max_ = 0xffffffff;
	static constexpr double invmax_ = 1.0 / (max_ + 1);

	std::array<mt_int, n_> state_;
	mt_int idx_{n_};

	void twist() {
		mt_int i = 0;
		for(; i < n_ - m_; i++) {
			mt_int y = (state_[i] & upper_mask_) | (state_[i + 1] & lower_mask_);
			state_[i] = state_[i + m_] ^ (y >> 1) ^ (y & 1 ? a_ : 0);
		}
		for(; i < n_ - 1; i++) {
			mt_int y = (state_[i] & upper_mask_) | (state_[i + 1] & lower_mask_);
			state_[i] = state_[i + (m_ - n_)] ^ (y >> 1) ^ (y & 1 ? a_ : 0);
		}
		mt_int y = (state_[n_ - 1] & upper_mask_) | (state_[0] & lower_mask_);
		state_[n_ - 1] = state_[m_ - 1] ^ (y >> 1) ^ (y & 1 ? a_ : 0);

		idx_ = 0;
	}

public:
	void seed(mt_int seed) {
		state_[0] = seed;
		for(mt_int i = 1; i < n_; i++) {
			state_[i] = f_ * (state_[i - 1] ^ (state_[i - 1] >> (w_ - 2))) + i;
		}
		idx_ = n_;
	}

	mt_int rng() {
		if(idx_ >= n_) {
			twist();
		}

		mt_int x = state_[idx_];
		x ^= ((x >> u_) & d_);
		x ^= ((x << s_) & b_);
		x ^= ((x << t_) & c_);
		x ^= (x >> l_);

		idx_++;
		return x;
	}

	double rng_double() { // returns [0, 1)
		return rng() * invmax_;
	}

	mt_int rng_integer(mt_int bound) { // returns [0, bound)
		return rng() / (max_ / bound + 1);
	}

	void save(std::vector<uint64_t> &state) const {
		state.resize(n_ + 1);
		std::copy(state_.begin(), state_.end(), state.begin());
		state.back() = n_ - idx_; // for backwards compatibility
	}

	void load(const std::vector<uint64_t> &state) {
		assert(state.size() == n_ + 1);
		std::copy(state.begin(), state.end() - 1, state_.begin());
		idx_ = n_ - state.back();
	}
};

}
