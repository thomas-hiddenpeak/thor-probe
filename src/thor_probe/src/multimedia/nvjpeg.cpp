#include "multimedia/nvjpeg.h"
#include "../include/probe_schema.h"
#include "communis/log.h"

#include <dlfcn.h>

namespace deusridet::probe {

GenericProbeComponent probe_nvjpeg() {
    GenericProbeComponent result;

#ifndef THOR_PROBE_ENABLE_NVJPEG
    result.status = "disabled";
    return result;
#endif

    void* lib = dlopen("libnvjpeg.so", RTLD_NOW | RTLD_LOCAL);
    if (!lib) {
        result.status = "sdk_unavailable";
        LOG_WARN("NvjpegProbe", "libnvjpeg.so not found: %s", dlerror());
        return result;
    }

    // Verify a known symbol exists
    auto nvjpegCreate = dlsym(lib, "nvjpegCreate");
    if (nvjpegCreate) {
        LOG_INFO("NvjpegProbe", "nvjpegCreate symbol found");
        result.status = "available";
    } else {
        LOG_WARN("NvjpegProbe", "nvjpegCreate not found in libnvjpeg.so");
        result.status = "sdk_unavailable";
    }

    dlclose(lib);
    return result;
}

} // namespace deusridet::probe
