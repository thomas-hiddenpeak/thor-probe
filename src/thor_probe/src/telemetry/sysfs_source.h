#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace deusridet::telemetry {

class SysfsSource {
public:
    SysfsSource() = default;
    ~SysfsSource() = default;

    bool is_available() const;

    // GPU clock: /sys/class/devfreq/gpu-gpc-{0,1,2}/cur_freq (Hz -> MHz)
    std::optional<unsigned int> gpu_gpc_clock_mhz(int gpc = 0) const;
    std::optional<unsigned int> gpu_nvd_clock_mhz() const;
    std::optional<unsigned int> gpu_load_pct() const;

    // Clock tree: /sys/kernel/debug/bpmp/debug/clk/<name>/rate (Hz -> MHz)
    std::optional<unsigned int> clock_hz(const std::string& clk_name) const;
    unsigned int clock_mhz(const std::string& clk_name) const;
    std::optional<unsigned int> nvenc_clock_mhz(int instance = 0) const;
    std::optional<unsigned int> nvdec_clock_mhz(int instance = 0) const;
    std::optional<unsigned int> vic_clock_mhz() const;
    std::optional<unsigned int> ofa_clock_mhz() const;
    std::optional<unsigned int> pva_clock_mhz() const;
    std::optional<unsigned int> emc_clock_mhz() const;

    // Thermal: /sys/devices/virtual/thermal/thermal_zone{0,1,2,...}/{type,temp}
    std::vector<std::pair<std::string, unsigned int>> all_thermal_zones() const;
    std::optional<unsigned int> thermal_zone_c(int index = 0) const;
    std::optional<unsigned int> thermal_zone_c(const std::string& name) const;

    // Power rails: scan /sys/bus/iio/devices/iio:device*/ina3221-in*_power (uW -> mW)
    std::vector<std::pair<std::string, unsigned int>> all_power_rails_mw() const;
    std::optional<unsigned int> power_rail_mw(const std::string& rail_name) const;

    // CPU: /sys/devices/system/cpu/cpu{0..}/cpufreq/scaling_cur_freq (kHz -> MHz)
    std::optional<unsigned int> cpu_core_mhz(int core = 0) const;
    int cpu_core_count() const;

    // devfreq available frequencies
    std::vector<unsigned int> available_frequencies_mhz(const std::string& dev_name) const;

private:
    static std::optional<std::string> read_file(const std::string& path);
    static std::optional<unsigned long> read_uint(const std::string& path);
};

} // namespace deusridet::telemetry
