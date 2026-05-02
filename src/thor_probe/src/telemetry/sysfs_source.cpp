#include "telemetry/sysfs_source.h"

#include "communis/log.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace deusridet::telemetry {

// ── helpers ──────────────────────────────────────────────────────────────────

std::optional<std::string> SysfsSource::read_file(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) return std::nullopt;
    std::string content;
    std::getline(ifs, content);
    return content;
}

std::optional<unsigned long> SysfsSource::read_uint(const std::string& path) {
    auto raw = read_file(path);
    if (!raw.has_value()) return std::nullopt;
    try {
        return static_cast<unsigned long>(std::stoul(raw.value()));
    } catch (...) {
        return std::nullopt;
    }
}

// ── availability ─────────────────────────────────────────────────────────────

bool SysfsSource::is_available() const {
    if (fs::exists("/sys/class/devfreq/gpu-gpc-0/cur_freq")) return true;
    if (fs::exists("/sys/devices/virtual/thermal/thermal_zone0/temp")) return true;
    return false;
}

// ── GPU clocks (devfreq) ────────────────────────────────────────────────────

std::optional<unsigned int> SysfsSource::gpu_gpc_clock_mhz(int gpc) const {
    auto val = read_uint("/sys/class/devfreq/gpu-gpc-" + std::to_string(gpc) + "/cur_freq");
    if (!val.has_value()) {
        LOG_WARN("sysfs", "gpu-gpc-%d cur_freq read failed", gpc);
        return std::nullopt;
    }
    return static_cast<unsigned int>(val.value() / 1000000);
}

std::optional<unsigned int> SysfsSource::gpu_nvd_clock_mhz() const {
    auto val = read_uint("/sys/class/devfreq/gpu-nvd-0/cur_freq");
    if (!val.has_value()) {
        LOG_WARN("sysfs", "gpu-nvd cur_freq read failed");
        return std::nullopt;
    }
    return static_cast<unsigned int>(val.value() / 1000000);
}

std::optional<unsigned int> SysfsSource::gpu_load_pct() const {
    auto val = read_uint("/sys/class/devfreq/gpu-gpc-0/loading");
    if (!val.has_value()) {
        LOG_WARN("sysfs", "gpu-gpc-0 loading read failed");
        return std::nullopt;
    }
    return static_cast<unsigned int>(val.value());
}

// ── clock tree (bpmp debugfs) ───────────────────────────────────────────────

std::optional<unsigned int> SysfsSource::clock_hz(const std::string& clk_name) const {
    auto val = read_uint("/sys/kernel/debug/bpmp/debug/clk/" + clk_name + "/rate");
    if (!val.has_value()) {
        LOG_WARN("sysfs", "clk/%s rate read failed", clk_name.c_str());
        return std::nullopt;
    }
    return static_cast<unsigned int>(val.value());
}

unsigned int SysfsSource::clock_mhz(const std::string& clk_name) const {
    auto hz = clock_hz(clk_name);
    if (!hz.has_value()) {
        LOG_WARN("sysfs", "clk/%s rate unavailable, returning 0", clk_name.c_str());
        return 0;
    }
    return static_cast<unsigned int>(hz.value() / 1000000);
}

std::optional<unsigned int> SysfsSource::nvenc_clock_mhz(int instance) const {
    auto hz = clock_hz("nvenc" + std::to_string(instance));
    return hz.has_value() ? std::make_optional<unsigned int>(hz.value() / 1000000) : std::nullopt;
}

std::optional<unsigned int> SysfsSource::nvdec_clock_mhz(int instance) const {
    auto hz = clock_hz("nvdec" + std::to_string(instance));
    return hz.has_value() ? std::make_optional<unsigned int>(hz.value() / 1000000) : std::nullopt;
}

std::optional<unsigned int> SysfsSource::vic_clock_mhz() const {
    auto hz = clock_hz("VIC");
    return hz.has_value() ? std::make_optional<unsigned int>(hz.value() / 1000000) : std::nullopt;
}

std::optional<unsigned int> SysfsSource::ofa_clock_mhz() const {
    auto hz = clock_hz("ofa");
    return hz.has_value() ? std::make_optional<unsigned int>(hz.value() / 1000000) : std::nullopt;
}

std::optional<unsigned int> SysfsSource::pva_clock_mhz() const {
    auto hz = clock_hz("pva");
    return hz.has_value() ? std::make_optional<unsigned int>(hz.value() / 1000000) : std::nullopt;
}

std::optional<unsigned int> SysfsSource::emc_clock_mhz() const {
    auto hz = clock_hz("emc");
    return hz.has_value() ? std::make_optional<unsigned int>(hz.value() / 1000000) : std::nullopt;
}

// ── thermal zones ────────────────────────────────────────────────────────────

std::vector<std::pair<std::string, unsigned int>> SysfsSource::all_thermal_zones() const {
    std::vector<std::pair<std::string, unsigned int>> zones;
    auto base = "/sys/devices/virtual/thermal";
    if (!fs::exists(base)) {
        LOG_WARN("sysfs", "thermal base %s not found", base);
        return zones;
    }
    try {
        for (const auto& entry : fs::directory_iterator(base)) {
            std::string name = entry.path().filename().string();
            if (name.find("thermal_zone") == std::string::npos) continue;
            auto type_str = read_file(entry.path() / "type");
            auto temp_str = read_file(entry.path() / "temp");
            if (!type_str.has_value() || !temp_str.has_value()) continue;
            try {
                unsigned int temp_c = static_cast<unsigned int>(std::stoul(temp_str.value()) / 1000);
                zones.push_back(std::make_pair(type_str.value(), temp_c));
            } catch (...) {
                LOG_WARN("sysfs", "thermal zone %s temp parse failed", name.c_str());
            }
        }
    } catch (const fs::filesystem_error& e) {
        LOG_WARN("sysfs", "thermal zone iteration error: %s", e.what());
    }
    return zones;
}

std::optional<unsigned int> SysfsSource::thermal_zone_c(int index) const {
    auto val = read_uint("/sys/devices/virtual/thermal/thermal_zone" + std::to_string(index) + "/temp");
    if (!val.has_value()) {
        LOG_WARN("sysfs", "thermal_zone%d temp read failed", index);
        return std::nullopt;
    }
    return static_cast<unsigned int>(val.value() / 1000);
}

std::optional<unsigned int> SysfsSource::thermal_zone_c(const std::string& name) const {
    auto zones = all_thermal_zones();
    for (const auto& [type, temp] : zones) {
        if (type == name) return temp;
    }
    LOG_WARN("sysfs", "thermal zone '%s' not found", name.c_str());
    return std::nullopt;
}

// ── power rails (IIO / INA3221) ─────────────────────────────────────────────

std::vector<std::pair<std::string, unsigned int>> SysfsSource::all_power_rails_mw() const {
    std::vector<std::pair<std::string, unsigned int>> rails;
    auto base = "/sys/bus/iio/devices";
    if (!fs::exists(base)) return rails;
    try {
        for (const auto& dev : fs::directory_iterator(base)) {
            std::string dev_name = dev.path().filename().string();
            if (dev_name.find("iio:device") == std::string::npos) continue;
            // Check if this device has ina3221 or power channels
            auto name_file = read_file(dev.path() / "name");
            bool is_ina = name_file.has_value() && name_file.value().find("ina3221") != std::string::npos;
            if (!is_ina) {
                // Also scan for _power files even without ina3221 name
                bool has_power = false;
                for (const auto& f : fs::directory_iterator(dev.path())) {
                    if (f.path().filename().string().find("_power") != std::string::npos) {
                        has_power = true;
                        break;
                    }
                }
                if (!has_power) continue;
            }
            // Scan power channels
            for (const auto& f : fs::directory_iterator(dev.path())) {
                std::string fname = f.path().filename().string();
                if (fname.find("_power") == std::string::npos) continue;
                auto val = read_uint(f.path().string());
                if (!val.has_value()) continue;
                // Value is in micro-watts (uW), convert to milli-watts (mW)
                unsigned int mw = static_cast<unsigned int>(val.value() / 1000);
                // Clean rail name: e.g. "ina3221-in0_power" -> "VDD_IN0"
                std::string rail_label = fname;
                auto in_pos = rail_label.find("in");
                if (in_pos != std::string::npos) {
                    std::string channel = rail_label.substr(in_pos);
                    channel.erase(channel.find("_power"));
                    rail_label = "VDD_" + channel;
                    for (auto& c : rail_label) c = static_cast<char>(toupper(c));
                }
                rails.push_back({rail_label, mw});
            }
        }
    } catch (const fs::filesystem_error& e) {
        LOG_WARN("sysfs", "power rail scan error: %s", e.what());
    }
    return rails;
}

std::optional<unsigned int> SysfsSource::power_rail_mw(const std::string& rail_name) const {
    auto rails = all_power_rails_mw();
    for (const auto& [name, mw] : rails) {
        if (name == rail_name) return mw;
    }
    LOG_WARN("sysfs", "power rail '%s' not found", rail_name.c_str());
    return std::nullopt;
}

// ── CPU frequencies ─────────────────────────────────────────────────────────

std::optional<unsigned int> SysfsSource::cpu_core_mhz(int core) const {
    auto val = read_uint("/sys/devices/system/cpu/cpu" + std::to_string(core) + "/cpufreq/scaling_cur_freq");
    if (!val.has_value()) {
        LOG_WARN("sysfs", "cpu%d scaling_cur_freq read failed", core);
        return std::nullopt;
    }
    // scaling_cur_freq is in kHz, convert to MHz
    return static_cast<unsigned int>(val.value() / 1000);
}

int SysfsSource::cpu_core_count() const {
    auto base = "/sys/devices/system/cpu";
    if (!fs::exists(base)) return 0;
    int count = 0;
    try {
        for (const auto& entry : fs::directory_iterator(base)) {
            std::string name = entry.path().filename().string();
            if (name.find("cpu") == 0 && name.size() > 3) {
                // Check rest is digits
                std::string digits = name.substr(3);
                bool all_digits = true;
                for (char c : digits) if (!isdigit(c)) { all_digits = false; break; }
                if (all_digits) count++;
            }
        }
    } catch (...) {}
    return count;
}

// ── devfreq available frequencies ────────────────────────────────────────────

std::vector<unsigned int> SysfsSource::available_frequencies_mhz(const std::string& dev_name) const {
    std::vector<unsigned int> freqs;
    auto content = read_file("/sys/class/devfreq/" + dev_name + "/available_frequencies");
    if (!content.has_value()) {
        LOG_WARN("sysfs", "devfreq %s available_frequencies not found", dev_name.c_str());
        return freqs;
    }
    std::istringstream iss(content.value());
    std::string token;
    while (std::getline(iss, token, ' ')) {
        token.erase(token.find_first_of('\t'));
        if (token.empty()) continue;
        try {
            // Values are in kHz, convert to MHz
            unsigned long khz = std::stoul(token);
            freqs.push_back(static_cast<unsigned int>(khz / 1000));
        } catch (...) {}
    }
    return freqs;
}

} // namespace deusridet::telemetry
