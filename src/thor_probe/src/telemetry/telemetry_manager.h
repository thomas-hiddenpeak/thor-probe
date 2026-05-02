#pragma once

#include "telemetry/sysfs_source.h"
#include "telemetry/tegrastats_source.h"
#include "probe_schema.h"

namespace deusridet::telemetry {

class TelemetryManager {
    SysfsSource sysfs_;
    TegraStatsSource tegra_stats_;

public:
    TelemetryManager() = default;

    std::vector<std::string> active_sources() const;

    unsigned int gpu_clock_mhz() const;
    unsigned int gpu_temp_c() const;
    unsigned int nvenc_clock_mhz(int instance = 0) const;
    unsigned int nvdec_clock_mhz(int instance = 0) const;
    unsigned int vic_clock_mhz() const;
    unsigned int ofa_clock_mhz() const;
    unsigned int pva_clock_mhz() const;
    unsigned int emc_bandwidth_mbps() const;
    std::pair<unsigned int, unsigned int> ram_usage_mb() const;

    deusridet::probe::TelemetrySnapshot snapshot() const;
};

} // namespace deusridet::telemetry
