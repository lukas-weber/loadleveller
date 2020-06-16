#pragma once

#include "evalable.h"
#include "results.h"
#include <filesystem>

namespace loadl {

// if rebinning_bin_length is 0, cbrt(total_sample_count) is used as default.
results merge(const std::vector<std::filesystem::path> &filenames, size_t rebinning_bin_length = 0,
              size_t skip = 0);
}
