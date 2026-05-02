#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "probe_schema.h"

namespace deusridet::probe {

int detect_warp_schedulers(int device = 0);
size_t detect_l1_cache_size(int device = 0);
std::pair<int, int> detect_shared_mem_banks(int device = 0);

/// Run occupancy curve analysis to infer register file organization.
/// Returns populated DeepSmResult (occupancy-derived fields).
DeepSmResult run_occupancy_probe(int device = 0);

/// Run ALL deep SM probes and return a complete DeepSmResult.
DeepSmResult run_deep_sm_probe(int device = 0);

} // namespace deusridet::probe
