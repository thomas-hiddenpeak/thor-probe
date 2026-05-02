#include "multimedia/ofa.h"
#include "../include/probe_schema.h"
#include "communis/log.h"

#include <dlfcn.h>
#include <cstdio>
#include <cstring>

namespace deusridet::probe {

OfaInfo probe_ofa() {
    OfaInfo result;

#ifndef THOR_PROBE_ENABLE_OFA
    result.status = "disabled";
    return result;
#endif

    void* lib = dlopen("libvpi.so", RTLD_NOW | RTLD_LOCAL);
    if (!lib) {
        result.status = "sdk_unavailable";
        LOG_WARN("OfaProbe", "libvpi.so not found: %s", dlerror());
        return result;
    }

    // Try to resolve vpiBackendGetAvailableBackends to check for VPI_BACKEND_OFA
    typedef void* (*vpiBackendGetAvailableBackends_t)(int*);
    auto vpiBackendGetAvailableBackends =
        reinterpret_cast<vpiBackendGetAvailableBackends_t>(dlsym(lib, "vpiBackendGetAvailableBackends"));

    bool ofa_found = false;
    if (vpiBackendGetAvailableBackends) {
        LOG_INFO("OfaProbe", "vpiBackendGetAvailableBackends symbol found");
        result.status = "available";
        // We can't easily inspect the backend enum values without VPI headers
        // but presence of the function + OFA feature flag suggests availability
        ofa_found = true;
    } else {
        // Check for VPI-specific symbols related to OFA
        auto vpiCreateStereoDisparity = dlsym(lib, "vpiAlgorithmCreate");
        if (vpiCreateStereoDisparity) {
            LOG_INFO("OfaProbe", "VPI algorithm creation available");
            result.status = "available";
            ofa_found = true;
        } else {
            LOG_WARN("OfaProbe", "No VPI OFA symbols found");
            result.status = "sdk_unavailable";
        }
    }

    if (ofa_found) {
        result.available = true;
        result.supports_optical_flow = true;
        result.supports_stereo_disparity = true;
    }

    // Attempt to read OFA clock from sysfs
    const char* sysfs_paths[] = {
        "/sys/kernel/debug/tegra_profiler/ofa_clk_rate",
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
            LOG_INFO("OfaProbe", "Clock from sysfs: %u MHz", result.clock_mhz);
        }
        fclose(fp);
    }

    dlclose(lib);
    return result;
}

} // namespace deusridet::probe
