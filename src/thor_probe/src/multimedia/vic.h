#pragma once

#include "../include/probe_schema.h"

namespace deusridet::probe {

/**
 * Probe VIC (Video Image Compression) capabilities.
 * Checks /sys/class/devfreq/ for VIC device and reads current frequency.
 */
GenericProbeComponent probe_vic();

} // namespace deusridet::probe
