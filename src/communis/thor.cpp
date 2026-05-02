/**
 * @file thor.cpp
 * @philosophical_role Thor hardware introspection. The entity's body is an NVIDIA Thor — knowing its power mode, thermal state, and clock profile is self-knowledge, not an abstraction leak.
 * @serves Actus (startup self-check), benchmark tools.
 */
#include "thor.h"
#include "log.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <cuda_runtime.h>

namespace deusridet {

struct FileCloser {
    void operator()(FILE* f) const { if (f) fclose(f); }
};
using FileGuard = std::unique_ptr<FILE, FileCloser>;

size_t read_memavail_kb() {
    size_t avail_kb = 0;
    FileGuard f(fopen("/proc/meminfo", "r"));
    if (!f) return 0;
    char line[256];
    while (fgets(line, sizeof(line), f.get())) {
        if (strncmp(line, "MemAvailable:", 13) == 0) {
            sscanf(line + 13, " %zu", &avail_kb);
            break;
        }
    }
    return avail_kb;
}

bool drop_page_caches() {
    FileGuard f(fopen("/proc/sys/vm/drop_caches", "w"));
    if (f) {
        int rc = fprintf(f.get(), "3\n");
        if (rc < 0) return false;
        return true;
    }
    return false;
}

void thor_cleanup() {
    size_t before_kb = read_memavail_kb();

    cudaError_t err = cudaDeviceReset();
    if (err != cudaSuccess) {
        LOG_ERROR("Thor", "cudaDeviceReset failed: %s", cudaGetErrorString(err));
    }

    if (!drop_page_caches()) {
        fprintf(stderr, "[WARN] Cannot write /proc/sys/vm/drop_caches "
                        "(need root). CMA pages may not be reclaimed.\n");
    }

    size_t after_kb = read_memavail_kb();
    if (before_kb > 0 && after_kb > 0) {
        long delta_mb = ((long)after_kb - (long)before_kb) / 1024;
        fprintf(stderr, "[Thor] MemAvailable: %zu MB -> %zu MB (%+ld MB)\n",
                before_kb / 1024, after_kb / 1024, delta_mb);
    }
}

ThorGpuInfo get_thor_gpu_info() {
    ThorGpuInfo info;

    cudaDeviceProp prop;
    cudaError_t err = cudaGetDeviceProperties(&prop, 0);
    if (err != cudaSuccess) {
        LOG_ERROR("Thor", "cudaGetDeviceProperties failed: %s", cudaGetErrorString(err));
        return info;
    }

    info.name                 = prop.name;
    info.compute_major        = prop.major;
    info.compute_minor        = prop.minor;
    info.total_mem            = prop.totalGlobalMem;
    info.shared_mem_per_sm    = prop.sharedMemPerMultiprocessor;
    info.multiprocessor_count = prop.multiProcessorCount;
    // clockRate removed from cudaDeviceProp in CUDA 13 — use attribute
    int clockRate = 0;
    err = cudaDeviceGetAttribute(&clockRate, cudaDevAttrClockRate, 0);
    if (err != cudaSuccess) {
        LOG_ERROR("Thor", "cudaDeviceGetAttribute(ClockRate) failed: %s", cudaGetErrorString(err));
    }
    info.clock_rate = clockRate;

    // tmem is available on SM110a (Blackwell-class)
    info.tmem_supported = (prop.major >= 11);

    LOG_INFO("Thor", "GPU: %s (SM%d.%d, %zu GB mem, %d SMs, %zu KB shared/SM, tmem=%s)",
             info.name.c_str(),
             info.compute_major, info.compute_minor,
             info.total_mem / (1024ULL * 1024 * 1024),
             info.multiprocessor_count,
             info.shared_mem_per_sm / 1024,
             info.tmem_supported ? "yes" : "no");

    return info;
}

bool validate_thor_gpu() {
    ThorGpuInfo info = get_thor_gpu_info();

    // Accept SM110a or any Blackwell-class GPU
    bool ok = (info.compute_major >= 11);

    if (!ok) {
        LOG_ERROR("Thor", "GPU %s (SM%d.%d) is NOT a Thor-compatible device (need SM110a+)",
                  info.name.c_str(), info.compute_major, info.compute_minor);
    } else {
        LOG_INFO("Thor", "GPU %s (SM%d.%d) validated as Thor-compatible",
                 info.name.c_str(), info.compute_major, info.compute_minor);
    }

    return ok;
}

} // namespace deusridet
