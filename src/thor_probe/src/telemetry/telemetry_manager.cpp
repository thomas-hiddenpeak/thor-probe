#include "telemetry/telemetry_manager.h"

#include "communis/log.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <sstream>

namespace deusridet::telemetry {

std::vector<std::string> TelemetryManager::active_sources() const {
    std::vector<std::string> sources;
    if (sysfs_.is_available()) sources.push_back("sysfs");
    if (tegra_stats_.is_available()) sources.push_back("tegrastats");
    return sources;
}

unsigned int TelemetryManager::gpu_clock_mhz() const {
    auto val = sysfs_.gpu_gpc_clock_mhz(0);
    if (val.has_value()) return val.value();
    return 0;
}

unsigned int TelemetryManager::gpu_temp_c() const {
    auto zones = sysfs_.all_thermal_zones();
    for (const auto& [type, temp] : zones) {
        if (type == "gpu-therm" || type == "soc-therm") return temp;
    }
    if (!zones.empty()) return zones[0].second;
    return 0;
}

unsigned int TelemetryManager::nvenc_clock_mhz(int instance) const {
    auto ts = tegra_stats_.query_once();
    if (instance < 2 && ts.nvenc_freq[instance].has_value()) {
        return ts.nvenc_freq[instance].value();
    }
    return sysfs_.clock_mhz("nvenc" + std::to_string(instance));
}

unsigned int TelemetryManager::nvdec_clock_mhz(int instance) const {
    auto ts = tegra_stats_.query_once();
    if (instance < 2 && ts.nvdec_freq[instance].has_value()) {
        return ts.nvdec_freq[instance].value();
    }
    return sysfs_.clock_mhz("nvdec" + std::to_string(instance));
}

unsigned int TelemetryManager::vic_clock_mhz() const {
    auto ts = tegra_stats_.query_once();
    if (ts.vic_freq.has_value() && !ts.vic_off) {
        return ts.vic_freq.value();
    }
    return sysfs_.clock_mhz("VIC");
}

unsigned int TelemetryManager::ofa_clock_mhz() const {
    auto ts = tegra_stats_.query_once();
    if (ts.ofa_freq.has_value()) return ts.ofa_freq.value();
    return sysfs_.clock_mhz("ofa");
}

unsigned int TelemetryManager::pva_clock_mhz() const {
    auto ts = tegra_stats_.query_once();
    if (ts.pva_freq.has_value() && !ts.pva_off) {
        return ts.pva_freq.value();
    }
    return sysfs_.clock_mhz("pva");
}

unsigned int TelemetryManager::emc_bandwidth_mbps() const {
    auto ts = tegra_stats_.query_once();
    if (ts.emc_bw_pct.has_value() && ts.emc_freq.has_value()) {
        // EMC bandwidth = (percentage * frequency) as a rough approximation
        return ts.emc_bw_pct.value() * ts.emc_freq.value();
    }
    return 0;
}

std::pair<unsigned int, unsigned int> TelemetryManager::ram_usage_mb() const {
    auto ts = tegra_stats_.query_once();
    if (ts.ram_used_mb.has_value() && ts.ram_total_mb.has_value()) {
        return {ts.ram_used_mb.value(), ts.ram_total_mb.value()};
    }
    // Fallback: /proc/meminfo
    std::ifstream meminfo("/proc/meminfo");
    if (!meminfo.is_open()) return {0, 0};
    unsigned long total_kb = 0, avail_kb = 0;
    std::string line;
    while (std::getline(meminfo, line)) {
        if (line.rfind("MemTotal:", 0) == 0) {
            std::istringstream iss(line);
            iss >> std::ws >> line;
            auto space = line.find(' ');
            if (space != std::string::npos) {
                try { total_kb = std::stoul(line.substr(0, space)); } catch (...) {}
            }
        } else if (line.rfind("MemAvailable:", 0) == 0) {
            std::istringstream iss(line);
            iss >> std::ws >> line;
            auto space = line.find(' ');
            if (space != std::string::npos) {
                try { avail_kb = std::stoul(line.substr(0, space)); } catch (...) {}
            }
        }
    }
    unsigned int used_mb = (total_kb - avail_kb) / 1024;
    unsigned int total_mb = total_kb / 1024;
    return {used_mb, total_mb};
}

deusridet::probe::TelemetrySnapshot TelemetryManager::snapshot() const {
    deusridet::probe::TelemetrySnapshot snap;

    // Timestamp
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    snap.timestamp_s = static_cast<double>(ts.tv_sec) + ts.tv_nsec / 1e9;

    // Determine sources
    std::string source_label;
    bool has_sysfs = sysfs_.is_available();
    bool has_tegra = tegra_stats_.is_available();
    if (has_sysfs && has_tegra) source_label = "fused";
    else if (has_tegra) source_label = "tegrastats";
    else if (has_sysfs) source_label = "sysfs";
    else source_label = "none";
    snap.source = source_label;

    // tegrastats snapshot (used by multiple fields)
    auto ts_result = tegra_stats_.query_once();

    // Clocks
    snap.clocks.gpu_mhz = gpu_clock_mhz();

    if (ts_result.gpu_gpc_freqs[0].has_value()) {
        snap.clocks.gpu_mhz = ts_result.gpu_gpc_freqs[0].value();
    }

    if (ts_result.emc_freq.has_value()) snap.clocks.emc_mhz = ts_result.emc_freq.value();

    if (ts_result.nvenc_freq[0].has_value()) snap.clocks.nvenc0_mhz = ts_result.nvenc_freq[0].value();
    if (ts_result.nvenc_freq[1].has_value()) snap.clocks.nvenc1_mhz = ts_result.nvenc_freq[1].value();
    if (ts_result.nvdec_freq[0].has_value()) snap.clocks.nvdec0_mhz = ts_result.nvdec_freq[0].value();
    if (ts_result.nvdec_freq[1].has_value()) snap.clocks.nvdec1_mhz = ts_result.nvdec_freq[1].value();
    if (ts_result.nvjpg_freq.has_value()) snap.clocks.nvjpg_mhz = ts_result.nvjpg_freq.value();
    if (ts_result.vic_freq.has_value() && !ts_result.vic_off) snap.clocks.vic_mhz = ts_result.vic_freq.value();
    if (ts_result.ofa_freq.has_value()) snap.clocks.ofa_mhz = ts_result.ofa_freq.value();
    if (ts_result.pva_freq.has_value() && !ts_result.pva_off) snap.clocks.pva_mhz = ts_result.pva_freq.value();
    if (ts_result.ape_freq.has_value()) snap.clocks.ape_mhz = ts_result.ape_freq.value();

    if (!ts_result.cpu_freqs.empty()) snap.clocks.cpu_mhz = ts_result.cpu_freqs[0];

    // Thermal
    auto zones = sysfs_.all_thermal_zones();
    for (const auto& [type, temp] : zones) {
        snap.thermal.all_zones.push_back({type, temp});
        if (type == "gpu-therm" || type == "gpu_therm") snap.thermal.gpu_temp_c = temp;
        if (type == "soc-therm" || type == "soc_therm" || type == "CPU_junction") snap.thermal.soc_temp_c = temp;
    }
    if (snap.thermal.gpu_temp_c == 0 && !zones.empty()) snap.thermal.gpu_temp_c = zones[0].second;

    // Power
    if (!ts_result.power_rails.empty()) {
        for (const auto& [name, pw] : ts_result.power_rails) {
            if (name.find("GPU") != std::string::npos) snap.power.gpu_mw = pw.first;
            if (name.find("SOC") != std::string::npos || name.find("CPU") != std::string::npos) snap.power.cpu_soc_mw = pw.first;
            if (name.find("VIN") != std::string::npos || name.find("5V") != std::string::npos) snap.power.vin_sys_mw = pw.first;
        }
    }
    if (ts_result.emc_bw_pct.has_value() && ts_result.emc_freq.has_value()) {
        snap.power.emc_bw_mbps = ts_result.emc_bw_pct.value() * ts_result.emc_freq.value();
    }

    // RAM
    if (ts_result.ram_used_mb.has_value() && ts_result.ram_total_mb.has_value()) {
        snap.ram_used_mb = ts_result.ram_used_mb.value();
        snap.ram_total_mb = ts_result.ram_total_mb.value();
    } else {
        auto ram = ram_usage_mb();
        snap.ram_used_mb = ram.first;
        snap.ram_total_mb = ram.second;
    }

    return snap;
}

} // namespace deusridet::telemetry
