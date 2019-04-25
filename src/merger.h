#pragma once

#include "evalable.h"
#include "results.h"

namespace loadl {

// if rebinning_bin_count is 0, cbrt(total_sample_count) is used as default.
results merge(const std::vector<std::string> &filenames, const std::vector<evalable> &evalables,
              size_t rebinning_bin_count = 0);
}
