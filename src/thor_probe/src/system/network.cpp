#include "system/network.h"

#include "communis/log.h"
#include "probe_schema.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace deusridet::probe {

NetworkInfo probe_network() {
    NetworkInfo result;

    auto net_path = "/sys/class/net";
    if (!fs::exists(net_path)) {
        LOG_WARN("probe_network", "%s not found", net_path);
        return result;
    }

    try {
        for (const auto& entry : fs::directory_iterator(net_path)) {
            if (!entry.is_directory()) continue;

            std::string iface_name = entry.path().filename().string();
            NetworkInterface iface;
            iface.name = iface_name;

            auto device_link = entry.path() / "device/driver";
            if (fs::exists(device_link)) {
                try {
                    iface.driver = fs::read_symlink(device_link).filename().string();
                } catch (const fs::filesystem_error& e) {
                    LOG_WARN("probe_network", "Cannot read driver symlink for %s: %s",
                             iface_name.c_str(), e.what());
                }
            }

            auto speed_path = entry.path() / "speed";
            std::ifstream speed_ifs(speed_path);
            if (speed_ifs.is_open()) {
                std::string speed_str;
                std::getline(speed_ifs, speed_str);
                while (!speed_str.empty() &&
                       (speed_str.back() == '\n' || speed_str.back() == '\r' ||
                        speed_str.back() == ' ')) {
                    speed_str.pop_back();
                }
                if (!speed_str.empty()) {
                    iface.speed = speed_str + " Mb/s";
                } else {
                    iface.speed = "unknown";
                }
            } else {
                iface.speed = "unknown";
            }

            result.interfaces.push_back(std::move(iface));
        }
    } catch (const fs::filesystem_error& e) {
        LOG_WARN("probe_network", "Error scanning %s: %s", net_path, e.what());
    }

    return result;
}

} // namespace deusridet::probe
