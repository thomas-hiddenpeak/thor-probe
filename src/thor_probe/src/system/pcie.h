#pragma once

#include "probe_schema.h"

namespace deusridet::probe {

using PcieInfo = decltype(SystemResult::pcie);

PcieInfo probe_pcie();

} // namespace deusridet::probe
