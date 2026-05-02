#include "system/display.h"

#include "communis/log.h"
#include "probe_schema.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace deusridet::probe {

DisplayInfo probe_display() {
    DisplayInfo result;

    auto drm_path = "/sys/class/drm";
    if (!fs::exists(drm_path)) {
        LOG_WARN("probe_display", "%s not found", drm_path);
        return result;
    }

    try {
        for (const auto& entry : fs::directory_iterator(drm_path)) {
            std::string name = entry.path().filename().string();
            if (name.find("card") != 0 || name.find("-") == std::string::npos) continue;

            auto status_path = entry.path() / "status";
            auto name_path = entry.path() / "name";
            std::ifstream status_ifs(status_path);
            if (!status_ifs.is_open()) {
                LOG_WARN("probe_display", "Cannot read status for %s", name.c_str());
                continue;
            }
            std::string status;
            std::getline(status_ifs, status);

            if (status != "connected") continue;
            std::string connector_name;
            std::ifstream name_ifs(name_path);
            if (name_ifs.is_open()) {
                std::getline(name_ifs, connector_name);
                while (!connector_name.empty() &&
                       (connector_name.back() == '\n' || connector_name.back() == '\r' ||
                        connector_name.back() == ' ')) {
                    connector_name.pop_back();
                }
            }

            if (connector_name.empty()) {
                connector_name = name;
            }

            result.outputs.push_back(connector_name);
        }
    } catch (const fs::filesystem_error& e) {
        LOG_WARN("probe_display", "Error scanning %s: %s", drm_path, e.what());
    }

    result.output_count = static_cast<int>(result.outputs.size());
    return result;
}

} // namespace deusridet::probe
