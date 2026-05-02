#pragma once

#include "../include/probe_schema.h"

namespace deusridet::probe {

/**
 * Probe NVJPEG codec availability.
 * Uses dlopen to check for libnvjpeg.so at runtime.
 */
GenericProbeComponent probe_nvjpeg();

} // namespace deusridet::probe
