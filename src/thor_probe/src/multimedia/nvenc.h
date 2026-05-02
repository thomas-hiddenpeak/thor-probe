#pragma once

#include "../include/probe_schema.h"

namespace deusridet::probe {

/**
 * Probe NVENC encoder capabilities.
 * Uses dlopen to load libnvidia-encode.so at runtime.
 * Falls back to hardcoded T5000 specs when SDK is unavailable.
 */
NvencCaps probe_nvenc();

} // namespace deusridet::probe
