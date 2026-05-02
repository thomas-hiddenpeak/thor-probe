#pragma once

#include "../include/probe_schema.h"

namespace deusridet::probe {

/**
 * Probe ISP (Image Signal Processor) capabilities.
 * Thor T5000 has 2 ISPs. Uses V4L2 device detection via sysfs.
 * @param index ISP index (0 or 1, default 0)
 */
GenericProbeComponent probe_isp(int index = 0);

} // namespace deusridet::probe
