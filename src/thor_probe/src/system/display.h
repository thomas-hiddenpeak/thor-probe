#pragma once

#include "probe_schema.h"

namespace deusridet::probe {

using DisplayInfo = decltype(SystemResult::display);

DisplayInfo probe_display();

} // namespace deusridet::probe
