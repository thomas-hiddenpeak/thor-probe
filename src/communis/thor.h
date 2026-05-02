/**
 * @file thor.h
 * @philosophical_role Declaration of the Thor introspection surface. Keeps hardware-specific knowledge funneled through one header so that subsystems never grep sysfs directly.
 * @serves Actus, benchmark tools, awaken startup.
 */
#pragma once

#include <cstddef>
#include <string>

namespace deusridet {

// Read MemAvailable from /proc/meminfo (returns kB, 0 on failure).
size_t read_memavail_kb();

// Write "3" to /proc/sys/vm/drop_caches.
// Tries direct write first, falls back to passwordless sudo -n.
bool drop_page_caches();

// cudaDeviceReset + drop_page_caches, with MemAvailable delta report.
void thor_cleanup();

// Query GPU device properties for Thor
struct ThorGpuInfo {
    std::string name;
    int         compute_major = 0;
    int         compute_minor = 0;
    size_t      total_mem = 0;
    size_t      shared_mem_per_sm = 0;
    int         multiprocessor_count = 0;
    int         clock_rate = 0;
    bool        tmem_supported = false;
};

ThorGpuInfo get_thor_gpu_info();

// Validate that current GPU is Thor-compatible (SM110a)
bool validate_thor_gpu();

} // namespace deusridet
