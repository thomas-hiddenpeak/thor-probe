#include "multimedia/nvdec.h"
#include "../include/probe_schema.h"
#include "communis/log.h"

#include <dlfcn.h>
#include <string>
#include <vector>

namespace deusridet::probe {

NvdecCaps probe_nvdec() {
    NvdecCaps result;

#ifndef THOR_PROBE_ENABLE_NVDEC
    result.status = "disabled";
    return result;
#endif

    void* lib = dlopen("libnvcuvid.so", RTLD_NOW | RTLD_LOCAL);
    if (!lib) {
        result.status = "sdk_unavailable";
        LOG_WARN("NvdecProbe", "libnvcuvid.so not found: %s", dlerror());
        // Fallback to hardcoded T5000 specs
        result.instance_count = 2;
        result.supported_codecs = {"H264", "HEVC", "AV1", "VP9"};
        result.limits = {{8192, 8192}};
        return result;
    }

    // Try to resolve cuvidCreateVideoParser symbol
    auto cuvidCreateVideoParser = dlsym(lib, "cuvidCreateVideoParser");
    if (cuvidCreateVideoParser) {
        LOG_INFO("NvdecProbe", "cuvidCreateVideoParser symbol found");
        result.status = "available";
    } else {
        LOG_WARN("NvdecProbe", "cuvidCreateVideoParser not found in libnvcuvid.so");
        result.status = "sdk_unavailable";
    }

    // T5000 NVDEC specifications
    result.instance_count = 2;
    result.supported_codecs = {"H264", "HEVC", "AV1", "VP9"};
    result.limits = {{8192, 8192}};

    dlclose(lib);
    return result;
}

} // namespace deusridet::probe
