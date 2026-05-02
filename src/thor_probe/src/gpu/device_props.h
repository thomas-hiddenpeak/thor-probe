#pragma once

#include "probe_schema.h"

namespace deusridet::probe {

/**
 * @brief Query GPU device properties via CUDA Runtime API.
 * @param device CUDA device index (default 0).
 * @return GpuDeviceProps with all queried properties and attributes.
 */
GpuDeviceProps query_device_props(int device = 0);

/**
 * @brief Refine device properties with deep SM microarchitecture probe results.
 * If dynamic probe results are available, they override the hardcoded CC lookup values.
 * @param props Device props from query_device_props (already populated)
 * @param deepSm Optional deep SM probe results
 * @return Updated GpuDeviceProps
 */
GpuDeviceProps refine_with_deep_sm(GpuDeviceProps props, std::optional<DeepSmResult> deepSm);

} // namespace deusridet::probe
