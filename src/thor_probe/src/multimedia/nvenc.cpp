#include "multimedia/nvenc.h"
#include "../include/probe_schema.h"
#include "communis/log.h"

#include <dlfcn.h>
#include <cstdio>
#include <fstream>
#include <string>

namespace deusridet::probe {

NvencCaps probe_nvenc() {
    NvencCaps result;

#ifndef THOR_PROBE_ENABLE_NVENC
    result.status = "disabled";
    return result;
#endif

    void* lib = dlopen("libnvidia-encode.so", RTLD_NOW | RTLD_LOCAL);
    if (!lib) {
        result.status = "sdk_unavailable";
        LOG_WARN("NvencProbe", "libnvidia-encode.so not found: %s", dlerror());
        result.instance_count = 2;
        result.h264_encode = true;
        result.hevc_encode = true;
        result.av1_encode = false;
        result.supported_presets = {"default", "hp", "hq", "bd", "ll", "llhq", "llhp"};
        result.max_bitrate_mbps = 480;
        return result;
    }

    typedef int (*NvEncodeAPIInitialize_t)(void*);
    auto NvEncodeAPIInitialize =
        reinterpret_cast<NvEncodeAPIInitialize_t>(dlsym(lib, "NvEncodeAPIInitialize"));

    if (NvEncodeAPIInitialize) {
        LOG_INFO("NvencProbe", "NvEncodeAPIInitialize symbol found");
        result.status = "available";
    } else {
        auto nvEncOpen = dlsym(lib, "nvEncOpen");
        if (nvEncOpen) {
            LOG_INFO("NvencProbe", "nvEncOpen symbol found");
            result.status = "available";
        } else {
            LOG_WARN("NvencProbe", "Neither NvEncodeAPIInitialize nor nvEncOpen found in libnvidia-encode.so");
            result.status = "sdk_unavailable";
        }
    }

    result.instance_count = 2;
    result.h264_encode = true;
    result.hevc_encode = true;
    result.av1_encode = false;
    result.supported_presets = {"default", "hp", "hq", "bd", "ll", "llhq", "llhp"};
    result.max_bitrate_mbps = 480;

    const char* sysfs_paths[] = {
        "/sys/kernel/debug/tegra_profiler/nvenc0_clk_rate",
        nullptr
    };
    for (int i = 0; sysfs_paths[i]; ++i) {
        std::ifstream ifs(sysfs_paths[i]);
        if (ifs.is_open()) {
            std::string line;
            if (std::getline(ifs, line)) {
                try {
                    unsigned long long rate = std::stoull(line);
                    result.clock_mhz = static_cast<unsigned int>(rate / 1000000);
                    LOG_INFO("NvencProbe", "Clock from sysfs: %u MHz", result.clock_mhz);
                    break;
                } catch (...) {
                    LOG_WARN("NvencProbe", "Failed to parse clock from %s", sysfs_paths[i]);
                }
            }
        }
    }

    dlclose(lib);
    return result;
}

} // namespace deusridet::probe
