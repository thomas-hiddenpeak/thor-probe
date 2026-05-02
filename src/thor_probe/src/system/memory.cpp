#include "system/memory.h"

#include "communis/log.h"
#include "probe_schema.h"

#include <fstream>
#include <string>

namespace deusridet::probe {

MemoryInfo probe_memory() {
    MemoryInfo result;
    result.type = "LPDDR5X";
    result.bus_width_bits = 256;
    result.peak_bandwidth_gb_s = 273.0;

    std::ifstream meminfo("/proc/meminfo");
    if (!meminfo.is_open()) {
        LOG_WARN("probe_memory", "Failed to open /proc/meminfo");
        return result;
    }

    std::string line;
    while (std::getline(meminfo, line)) {
        if (line.empty()) continue;
        if (line.find("MemTotal") == 0) {
            try {
                size_t num_start = line.find_first_of("0123456789");
                if (num_start != std::string::npos) {
                    unsigned long kb = std::stoul(line.substr(num_start));
                    result.total_bytes = kb * 1024UL;
                } else {
                    LOG_WARN("probe_memory", "Failed to parse MemTotal value");
                }
            } catch (const std::exception& e) {
                LOG_WARN("probe_memory", "MemTotal parse error: %s", e.what());
            }
            break;
        }
    }

    if (result.total_bytes == 0) {
        LOG_WARN("probe_memory", "MemTotal not found or parse failed");
    }

    return result;
}

} // namespace deusridet::probe
