#pragma once

#include "../include/probe_schema.h"

namespace deusridet::probe {

/**
 * Probe NVDEC decoder capabilities.
 * Uses dlopen to load libnvcuvid.so at runtime.
 * Falls back to hardcoded T5000 specs when SDK is unavailable.
 */
NvdecCaps probe_nvdec();

} // namespace deusridet::probe
