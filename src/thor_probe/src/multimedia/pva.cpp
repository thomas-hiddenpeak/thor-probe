#include "multimedia/pva.h"
#include "../include/probe_schema.h"
#include "communis/log.h"

#include <dlfcn.h>
#include <cstdio>

namespace deusridet::probe {

PvaInfo probe_pva() {
    PvaInfo result;

#ifndef THOR_PROBE_ENABLE_PVA
    result.status = "disabled";
    return result;
#endif

    void* lib = dlopen("libcuppva.so", RTLD_NOW | RTLD_LOCAL);
    if (!lib) {
        result.status = "sdk_unavailable";
        LOG_WARN("PvaProbe", "libcuppva.so not found: %s", dlerror());
        // Fallback to hardcoded T5000 specs
        result.engine_count = 1;
        result.vpu_count = 2;
        result.generation = "GEN3";
        result.int8_gmac_s = 2488;    // per VPU
        result.fp16_gflops_s = 622;   // per VPU
        return result;
    }

    // Try to resolve CupvaGetHardwareInfo symbol
    auto CupvaGetHardwareInfo = dlsym(lib, "CupvaGetHardwareInfo");
    if (CupvaGetHardwareInfo) {
        LOG_INFO("PvaProbe", "CupvaGetHardwareInfo symbol found");
        result.status = "available";
    } else {
        LOG_WARN("PvaProbe", "CupvaGetHardwareInfo not found in libcuppva.so");
        result.status = "sdk_unavailable";
    }

    // T5000 PVA specifications (GEN3)
    result.engine_count = 1;
    result.vpu_count = 2;
    result.generation = "GEN3";
    result.int8_gmac_s = 2488;    // per VPU
    result.fp16_gflops_s = 622;   // per VPU

    // Attempt to read PVA clock from sysfs
    const char* sysfs_paths[] = {
        "/sys/kernel/debug/tegra_profiler/pva_clk_rate",
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
            LOG_INFO("PvaProbe", "Clock from sysfs: %u MHz", result.clock_mhz);
        }
        fclose(fp);
    }

    dlclose(lib);
    return result;
}

} // namespace deusridet::probe
