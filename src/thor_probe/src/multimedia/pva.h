#pragma once

#include "../include/probe_schema.h"

namespace deusridet::probe {

/**
 * Probe PVA (Programmable Video Accelerator) capabilities.
 * Uses dlopen to load libcuppva.so at runtime.
 * Falls back to hardcoded T5000 specs when SDK is unavailable.
 */
PvaInfo probe_pva();

} // namespace deusridet::probe
