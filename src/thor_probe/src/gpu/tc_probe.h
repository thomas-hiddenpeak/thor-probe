#pragma once

#include "probe_schema.h"

namespace deusridet::probe {

/**
 * @brief Detect tcgen05 Tensor Core capabilities via kernel probes.
 * @param device CUDA device index (default 0).
 * @return TcGen05Capability with all detected MMA, barrier, TMA, and warp features.
 */
TcGen05Capability detect_tcgen05_capabilities(int device = 0);

} // namespace deusridet::probe
