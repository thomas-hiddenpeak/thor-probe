#pragma once

#include "probe_schema.h"

namespace deusridet::probe {

using NetworkInfo = decltype(SystemResult::network);

NetworkInfo probe_network();

} // namespace deusridet::probe
