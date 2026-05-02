#pragma once

#include "probe_schema.h"

namespace deusridet::probe {

using MemoryInfo = decltype(SystemResult::memory);

MemoryInfo probe_memory();

} // namespace deusridet::probe
