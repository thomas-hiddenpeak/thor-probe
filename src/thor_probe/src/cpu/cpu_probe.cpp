#include "cpu_probe.h"
#include "communis/log.h"
#include "probe_schema.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace deusridet::probe {

static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::string read_file(const std::filesystem::path& path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) return "";
    std::string content((std::istreambuf_iterator<char>(ifs)),
                         std::istreambuf_iterator<char>());
    return trim(content);
}

static std::string implementer_name(const std::string& hex) {
    if (hex == "0x41") return "ARM";
    if (hex == "0x43") return "Cavium";
    if (hex == "0x4d") return "Intel";
    if (hex == "0x4e") return "NVIDIA";
    if (hex == "0x51") return "Qualcomm";
    if (hex == "0xc0") return "Broadcom";
    return "unknown";
}

static std::string cpu_part_name(unsigned int part) {
    static const std::unordered_map<unsigned int, std::string> parts = {
        {0xd4f, "Cortex-X1"},
        {0xd83, "Cortex-X series"},
        {0xd80, "Cortex-X3"},
        {0xd4c, "Cortex-A78"},
        {0xd4d, "Cortex-A78AE"},
        {0xd08, "Cortex-A76"},
        {0xd0b, "Cortex-A65"},
        {0xd47, "Cortex-A55"},
        {0xd46, "Cortex-A76 AE"},
        {0xc20, "Cortex-A76 (early)"},
        {0xd48, "Cortex-A77"},
        {0xd49, "Cortex-A76AE"},
        {0xd4b, "Cortex-A710"},
        {0xd4e, "Cortex-A715"},
        {0xd44, "Cortex-A35"},
        {0xd40, "Cortex-A510"},
        {0xd41, "Cortex-A520"},
        {0xd22, "Aurora (ThunderX2)"},
    };
    auto it = parts.find(part);
    return it != parts.end() ? it->second : "unknown";
}

static std::vector<std::unordered_map<std::string, std::string>>
parse_cpuinfo() {
    std::vector<std::unordered_map<std::string, std::string>> processors;
    std::unordered_map<std::string, std::string>* current = nullptr;

    std::ifstream ifs("/proc/cpuinfo");
    if (!ifs.is_open()) {
        LOG_WARN("CpuProbe", "Cannot open /proc/cpuinfo");
        return processors;
    }

    std::string line;
    while (std::getline(ifs, line)) {
        if (trim(line).empty()) continue;

        auto sep = line.find(": ");
        if (sep == std::string::npos) {
            sep = line.find(':');
            if (sep == std::string::npos) continue;
        }

        std::string key = trim(line.substr(0, sep));
        size_t val_start = (sep == line.find(": ")) ? sep + 2 : sep + 1;
        std::string value = trim(line.substr(val_start));

        if (key == "processor") {
            processors.emplace_back();
            current = &processors.back();
            (*current)["processor"] = value;
        } else if (current) {
            (*current)[key] = value;
        }
    }

    return processors;
}

static int count_cpus_from_sysfs() {
    std::string present = read_file("/sys/devices/system/cpu/present");
    if (!present.empty()) {
        auto dash = present.find('-');
        if (dash != std::string::npos) {
            try {
                int max = std::stoi(present.substr(dash + 1));
                return max + 1;
            } catch (...) {}
        }
    }

    int count = 0;
    auto cpu_base = std::filesystem::path("/sys/devices/system/cpu");
    if (std::filesystem::exists(cpu_base)) {
        for (auto& entry : std::filesystem::directory_iterator(cpu_base)) {
            if (entry.is_directory()) {
                std::string name = entry.path().filename().string();
                if (name.size() > 3 && name.substr(0, 3) == "cpu") {
                    count++;
                }
            }
        }
    }
    return count;
}

static CpuResult::CacheInfo parse_cache_from_sysfs() {
    CpuResult::CacheInfo cache;
    auto cpu0_cache = std::filesystem::path("/sys/devices/system/cpu/cpu0/cache");
    if (!std::filesystem::exists(cpu0_cache)) return cache;

    for (auto& entry : std::filesystem::directory_iterator(cpu0_cache)) {
        if (!entry.is_directory()) continue;
        auto idx = entry.path().filename().string();
        if (idx.substr(0, 5) != "index") continue;

        std::string level_str = read_file(entry.path() / "level");
        if (level_str.empty()) continue;
        int level = std::stoi(level_str);

        std::string type = read_file(entry.path() / "type");
        std::string size_str = read_file(entry.path() / "size");

        size_t size_kb = 0;
        {
            std::string trimmed = size_str;
            while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t')) {
                trimmed.pop_back();
            }
            if (!trimmed.empty()) {
                char suffix = trimmed.back();
                trimmed.pop_back();
                try {
                    size_t val = std::stoull(trimmed);
                    if (suffix == 'K' || suffix == 'k') size_kb = val;
                    else if (suffix == 'M' || suffix == 'm') size_kb = val * 1024;
                    else if (suffix == 'G' || suffix == 'g') size_kb = val * 1024 * 1024;
                } catch (...) {}
            }
        }

        if (level == 1 && type == "Data") {
            cache.l1d_per_core_kb = size_kb;
        } else if (level == 1 && type == "Instruction") {
            cache.l1i_per_core_kb = size_kb;
        } else if (level == 2) {
            cache.l2_per_cluster_kb = size_kb;
        } else if (level == 3) {
            cache.l3_total_kb = size_kb;
        }
    }

    return cache;
}

static std::string get_platform_name() {
    std::string platform = read_file("/sys/devices/soc0/machine");
    if (platform.empty()) {
        platform = read_file("/sys/firmware/devicetree/base/model");
    }
    return platform;
}

CpuResult probe_cpu() {
    CpuResult result;
    result.platform = get_platform_name();

    auto processors = parse_cpuinfo();
    bool cpuinfo_available = !processors.empty();

    if (cpuinfo_available) {
        auto& first = processors.front();

        std::string cpu_part_raw;
        std::string cpu_implementer_raw;

        if (first.count("CPU part")) {
            cpu_part_raw = first["CPU part"];
            result.cpu_part = cpu_part_raw;

            unsigned int part = 0;
            try {
                part = static_cast<unsigned int>(std::stoul(cpu_part_raw, nullptr, 16));
            } catch (...) {
                part = 0;
            }
            result.model_name = cpu_part_name(part);
        }

        if (first.count("CPU implementer")) {
            cpu_implementer_raw = first["CPU implementer"];
            result.cpu_implementer = cpu_implementer_raw + " (" +
                                     implementer_name(cpu_implementer_raw) + ")";
        }

        if (first.count("CPU architecture")) {
            std::string arch_ver = first["CPU architecture"];
            if (arch_ver == "8") {
                result.architecture = "aarch64";
            } else {
                result.architecture = "armv" + arch_ver;
            }
        } else {
            result.architecture = "aarch64";
        }

        if (result.model_name.empty() && first.count("Model name")) {
            result.model_name = first["Model name"];
        }

        std::set<int> core_ids;
        std::set<int> physical_ids;
        std::vector<CpuCoreInfo> cores;
        cores.reserve(processors.size());

        for (auto& proc : processors) {
            CpuCoreInfo core;
            if (proc.count("processor")) {
                core.core_id = std::stoi(proc["processor"]);
            }
            core_ids.insert(core.core_id);

            if (proc.count("Physical id")) {
                core.physical_id = std::stoi(proc["Physical id"]);
                physical_ids.insert(core.physical_id);
            } else {
                core.physical_id = 0;
            }

            if (proc.count("Features")) {
                core.features = proc["Features"];
            }

            if (proc.count("BogoMIPS")) {
                try {
                    core.bogomips = static_cast<unsigned int>(
                        std::stof(proc["BogoMIPS"]) * 100);
                } catch (...) {
                    core.bogomips = 0;
                }
            }

            cores.push_back(std::move(core));
        }

        unsigned int global_min_mhz = UINT32_MAX;
        unsigned int global_max_mhz = 0;

        for (auto& core : cores) {
            auto base = std::filesystem::path("/sys/devices/system/cpu") /
                        ("cpu" + std::to_string(core.core_id));

            {
                std::string freq_str = read_file(base / "cpufreq" / "scaling_cur_freq");
                if (!freq_str.empty()) {
                    core.mhz = static_cast<unsigned int>(std::stoul(freq_str) / 1000);
                }
            }

            {
                std::string max_str = read_file(base / "cpufreq" / "cpuinfo_max_freq");
                if (!max_str.empty()) {
                    core.max_mhz = static_cast<unsigned int>(std::stoul(max_str) / 1000);
                    global_max_mhz = std::max(global_max_mhz, core.max_mhz);
                }
            }

            {
                std::string min_str = read_file(base / "cpufreq" / "cpuinfo_min_freq");
                if (!min_str.empty()) {
                    core.min_mhz = static_cast<unsigned int>(std::stoul(min_str) / 1000);
                    if (core.min_mhz < global_min_mhz) {
                        global_min_mhz = core.min_mhz;
                    }
                }
            }
        }

        result.cpu_min_mhz = global_min_mhz == UINT32_MAX ? 0 : global_min_mhz;
        result.cpu_max_mhz = global_max_mhz;
        result.core_count = static_cast<int>(core_ids.size());
        result.physical_id_count = physical_ids.empty() ? 1 : static_cast<int>(physical_ids.size());
        result.cores = std::move(cores);

    } else {
        LOG_WARN("CpuProbe", "/proc/cpuinfo unavailable, falling back to sysfs");

        result.architecture = "aarch64";
        result.core_count = count_cpus_from_sysfs();

        {
            std::string max_str = read_file("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");
            std::string min_str = read_file("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_min_freq");
            if (!max_str.empty()) {
                result.cpu_max_mhz = static_cast<unsigned int>(std::stoul(max_str) / 1000);
            }
            if (!min_str.empty()) {
                result.cpu_min_mhz = static_cast<unsigned int>(std::stoul(min_str) / 1000);
            }
        }
    }

    result.cache = parse_cache_from_sysfs();

    return result;
}

} // namespace deusridet::probe
