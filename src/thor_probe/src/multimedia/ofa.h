#pragma once

#include "../include/probe_schema.h"

namespace deusridet::probe {

/**
 * Probe OFA (Optical Flow Accelerator) capabilities.
 * Uses dlopen to load libvpi.so at runtime and checks for VPI_BACKEND_OFA.
 */
OfaInfo probe_ofa();

} // namespace deusridet::probe
