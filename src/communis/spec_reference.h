#pragma once

#include <string>
#include <optional>
#include <cstdint>

namespace deusridet::probe {

struct SpecReference {
    std::string model;            // "T5000" or "T4000"
    std::string source;           // "NVIDIA DS-11945-001 v1.2"

    // GPU
    int gpu_sm_count = 0;
    int gpu_cuda_cores = 0;
    int gpu_tensor_cores = 0;
    double gpu_boost_clock_ghz = 0.0;  // MAXN
    size_t gpu_l2_cache_bytes = 0;
    size_t gpu_smem_per_sm_bytes = 0;
    int gpu_tmus = 0;
    int gpu_rops = 0;

    // CPU
    int cpu_core_count = 0;
    double cpu_max_freq_ghz = 0.0;
    size_t cpu_l1d_per_core_kb = 0;
    size_t cpu_l1i_per_core_kb = 0;
    size_t cpu_l2_per_core_kb = 0;
    size_t cpu_l3_total_kb = 0;

    // Memory
    size_t memory_total_bytes = 0;
    std::string memory_type;
    double memory_peak_bw_gb_s = 0.0;
    int memory_bus_width_bits = 0;

    // Multimedia
    int nvenc_instance_count = 0;
    int nvdec_instance_count = 0;
    double pva_clock_ghz = 0.0;
    double nvenc_clock_max_ghz = 0.0;
    double nvdec_clock_max_ghz = 0.0;

    // I/O (SoC level)
    int pcie_version = 0;
    int pcie_max_lanes = 0;
};

/**
 * Get spec reference for the given model.
 * Returns empty optional if model not recognized.
 */
std::optional<SpecReference> get_spec_reference(const std::string& model);

/**
 * Get spec reference for T5000 specifically (convenience).
 */
std::optional<SpecReference> get_spec_t5000();

/**
 * Get spec reference for T4000 specifically (convenience).
 */
std::optional<SpecReference> get_spec_t4000();

} // namespace deusridet::probe
