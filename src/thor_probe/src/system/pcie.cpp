#include "system/pcie.h"

#include "communis/log.h"
#include "probe_schema.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace deusridet::probe {

PcieInfo probe_pcie() {
    PcieInfo result;
    result.version = "Gen5";

    auto pci_base = "/sys/bus/pci/devices";
    if (!fs::exists(pci_base)) {
        LOG_WARN("probe_pcie", "%s not found", pci_base);
        return result;
    }

    try {
        for (const auto& entry : fs::directory_iterator(pci_base)) {
            if (!entry.is_directory()) continue;

            std::string dev_name = entry.path().filename().string();
            PciePort port;
            port.name = dev_name;

            auto speed_path = entry.path() / "current_link_speed";
            std::ifstream speed_ifs(speed_path);
            if (speed_ifs.is_open()) {
                std::string speed_str;
                std::getline(speed_ifs, speed_str);
                while (!speed_str.empty() &&
                       (speed_str.back() == '\n' || speed_str.back() == '\r' ||
                        speed_str.back() == ' ')) {
                    speed_str.pop_back();
                }
                try {
                    size_t dot_pos = speed_str.find('.');
                    std::string num_part = (dot_pos != std::string::npos)
                        ? speed_str.substr(0, dot_pos)
                        : speed_str.substr(0, speed_str.find(' '));
                    port.link_speed = std::stoi(num_part);
                } catch (...) {
                    port.link_speed = 0;
                }
            } else {
                port.link_speed = 0;
            }

            auto width_path = entry.path() / "current_link_width";
            std::ifstream width_ifs(width_path);
            if (width_ifs.is_open()) {
                std::string width_str;
                std::getline(width_ifs, width_str);
                while (!width_str.empty() &&
                       (width_str.back() == '\n' || width_str.back() == '\r' ||
                        width_str.back() == ' ')) {
                    width_str.pop_back();
                }
                try {
                    size_t num_start = width_str.find_first_of("0123456789");
                    if (num_start != std::string::npos) {
                        port.link_width = std::stoi(width_str.substr(num_start));
                    } else {
                        port.link_width = 0;
                    }
                } catch (...) {
                    port.link_width = 0;
                }
            } else {
                port.link_width = 0;
            }

            result.ports.push_back(std::move(port));
        }
    } catch (const fs::filesystem_error& e) {
        LOG_WARN("probe_pcie", "Error scanning %s: %s", pci_base, e.what());
    }

    return result;
}

} // namespace deusridet::probe
