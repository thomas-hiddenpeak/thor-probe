#include "multimedia/vic.h"
#include "../include/probe_schema.h"
#include "communis/log.h"

#include <cstdio>
#include <dirent.h>
#include <fstream>
#include <string>
#include <cstring>

namespace deusridet::probe {

GenericProbeComponent probe_vic() {
    GenericProbeComponent result;

    // Primary: check devfreq for VIC device on T5000
    // The VIC devfreq path on Thor T5000: /sys/class/devfreq/8188050000.vic/cur_freq
    const char* cur_freq_path = "/sys/class/devfreq/8188050000.vic/cur_freq";
    {
        std::ifstream ifs(cur_freq_path);
        if (ifs.is_open()) {
            std::string line;
            if (std::getline(ifs, line)) {
                try {
                    unsigned long long freq_hz = std::stoull(line);
                    result.clock_mhz = static_cast<unsigned int>(freq_hz / 1000000);
                    LOG_INFO("VicProbe", "Clock from devfreq: %u MHz", result.clock_mhz);
                    result.status = "available";
                    return result;
                } catch (...) {
                    LOG_WARN("VicProbe", "Failed to parse cur_freq from %s", cur_freq_path);
                    result.status = "unavailable";
                    return result;
                }
            }
        }
    }

    // Fallback: search for any *.vic directory under /sys/class/devfreq/
    DIR* dir = opendir("/sys/class/devfreq/");
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            std::string name = entry->d_name;
            if (name.size() >= 4 && name.substr(name.size() - 4) == ".vic") {
                std::string path = std::string("/sys/class/devfreq/") + entry->d_name + "/cur_freq";
                std::ifstream ifs(path.c_str());
                if (ifs.is_open()) {
                    std::string line;
                    if (std::getline(ifs, line)) {
                        try {
                            unsigned long long freq_hz = std::stoull(line);
                            result.clock_mhz = static_cast<unsigned int>(freq_hz / 1000000);
                            LOG_INFO("VicProbe", "Clock from %s: %u MHz", path.c_str(), result.clock_mhz);
                            result.status = "available";
                        } catch (...) {
                            LOG_WARN("VicProbe", "Failed to parse clock from %s", path.c_str());
                        }
                    }
                    closedir(dir);
                    return result;
                }
            }
        }
        closedir(dir);
    }

    // Fallback: check /sys/kernel/debug/tegra_profiler/ for VIC clock
    const char* fallback_paths[] = {
        "/sys/kernel/debug/tegra_profiler/vic_clk_rate",
        nullptr
    };
    for (int i = 0; fallback_paths[i]; ++i) {
        std::ifstream ifs(fallback_paths[i]);
        if (ifs.is_open()) {
            std::string line;
            if (std::getline(ifs, line)) {
                try {
                    unsigned long long rate = std::stoull(line);
                    result.clock_mhz = static_cast<unsigned int>(rate / 1000000);
                    LOG_INFO("VicProbe", "Clock from %s: %u MHz", fallback_paths[i], result.clock_mhz);
                    result.status = "available";
                } catch (...) {
                    LOG_WARN("VicProbe", "Failed to parse clock from %s", fallback_paths[i]);
                }
            }
            return result;
        }
    }

    result.status = "unavailable";
    LOG_WARN("VicProbe", "VIC device not found via devfreq or sysfs");
    return result;
}

} // namespace deusridet::probe
