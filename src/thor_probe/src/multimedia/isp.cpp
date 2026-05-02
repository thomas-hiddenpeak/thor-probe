#include "multimedia/isp.h"
#include "../include/probe_schema.h"
#include "communis/log.h"

#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <string>
#include <sys/stat.h>

namespace deusridet::probe {

GenericProbeComponent probe_isp(int index) {
    GenericProbeComponent result;

    if (index < 0 || index > 1) {
        result.status = "invalid_index";
        LOG_WARN("IspProbe", "Invalid ISP index %d (must be 0 or 1)", index);
        return result;
    }

    // Check V4L2 devices under /sys/class/video4linux/
    std::string v4l2_base = "/sys/class/video4linux/";
    std::string device_name = "ispvideo" + std::to_string(index);

    bool found = false;
    DIR* dir = opendir(v4l2_base.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            if (entry->d_name == device_name) {
                found = true;
                LOG_INFO("IspProbe", "Found %s in %s", device_name.c_str(), v4l2_base.c_str());
                break;
            }
        }
        closedir(dir);
    } else {
        LOG_WARN("IspProbe", "Cannot open %s", v4l2_base.c_str());
    }

    if (!found) {
        // Also check for videoN devices that may correspond to ISP
        std::string dev_path = "/dev/" + device_name;
        struct stat st;
        if (stat(dev_path.c_str(), &st) == 0 && S_ISCHR(st.st_mode)) {
            found = true;
            LOG_INFO("IspProbe", "Found /dev/%s as character device", device_name.c_str());
        }
    }

    if (found) {
        result.status = "available";
    } else {
        result.status = "not_found";
        LOG_WARN("IspProbe", "ISP%d device not found via V4L2", index);
    }

    // Attempt to read ISP clock from sysfs
    // Common paths for Tegra ISP clocks
    const char* sysfs_paths[] = {
        "/sys/kernel/debug/tegra_profiler/isp_clk_rate",
        nullptr
    };
    FILE* fp = nullptr;
    for (int i = 0; sysfs_paths[i] && !fp; ++i) {
        fp = fopen(sysfs_paths[i], "r");
    }
    if (fp) {
        unsigned long long rate = 0;
        if (fscanf(fp, "%llu", &rate) == 1) {
            result.clock_mhz = static_cast<unsigned int>(rate / 1000000);
            LOG_INFO("IspProbe", "Clock from sysfs: %u MHz", result.clock_mhz);
        }
        fclose(fp);
    }

    return result;
}

} // namespace deusridet::probe
