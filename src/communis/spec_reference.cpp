#include "spec_reference.h"

namespace deusridet::probe {

// ============================================================================
// T5000 — NVIDIA DS-11945-001 v1.2
// ============================================================================
static SpecReference make_t5000() {
    SpecReference s;
    s.model = "T5000";
    s.source = "NVIDIA DS-11945-001 v1.2";

    // GPU
    s.gpu_sm_count = 20;
    s.gpu_cuda_cores = 2560;
    s.gpu_tensor_cores = 96;          // 5th gen
    s.gpu_boost_clock_ghz = 1.575;    // MAXN
    s.gpu_l2_cache_bytes = 50 * 1024 * 1024;  // ~50 MB (third-party)
    s.gpu_smem_per_sm_bytes = 256 * 1024;       // 256 KB/SM (Blackwell SM)
    s.gpu_tmus = 96;
    s.gpu_rops = 32;

    // CPU
    s.cpu_core_count = 14;
    s.cpu_max_freq_ghz = 2.6;
    s.cpu_l1d_per_core_kb = 64;
    s.cpu_l1i_per_core_kb = 64;
    s.cpu_l2_per_core_kb = 1024;  // 1 MB/core
    s.cpu_l3_total_kb = 16 * 1024;  // 16 MB

    // Memory
    s.memory_total_bytes = 128ULL * 1024 * 1024 * 1024;  // 128 GB
    s.memory_type = "LPDDR5X";
    s.memory_peak_bw_gb_s = 273.0;
    s.memory_bus_width_bits = 256;

    // Multimedia
    s.nvenc_instance_count = 2;
    s.nvdec_instance_count = 2;
    s.pva_clock_ghz = 1.215;
    s.nvenc_clock_max_ghz = 1.69;
    s.nvdec_clock_max_ghz = 1.69;

    // I/O
    s.pcie_version = 5;
    s.pcie_max_lanes = 8;

    return s;
}

// ============================================================================
// T4000 — NVIDIA DS-11945-001
// ============================================================================
static SpecReference make_t4000() {
    SpecReference s;
    s.model = "T4000";
    s.source = "NVIDIA DS-11945-001";

    // GPU
    s.gpu_sm_count = 12;
    s.gpu_cuda_cores = 1536;
    s.gpu_tensor_cores = 64;          // 5th gen
    s.gpu_boost_clock_ghz = 1.53;     // MAXN
    s.gpu_l2_cache_bytes = 50 * 1024 * 1024;  // ~50 MB
    s.gpu_smem_per_sm_bytes = 256 * 1024;       // 256 KB/SM (Blackwell SM)
    s.gpu_tmus = 64;
    s.gpu_rops = 16;

    // CPU
    s.cpu_core_count = 12;
    s.cpu_max_freq_ghz = 2.6;
    s.cpu_l1d_per_core_kb = 64;
    s.cpu_l1i_per_core_kb = 64;
    s.cpu_l2_per_core_kb = 1024;  // 1 MB/core
    s.cpu_l3_total_kb = 16 * 1024;  // 16 MB

    // Memory
    s.memory_total_bytes = 64ULL * 1024 * 1024 * 1024;  // 64 GB
    s.memory_type = "LPDDR5X";
    s.memory_peak_bw_gb_s = 273.0;
    s.memory_bus_width_bits = 256;

    // Multimedia
    s.nvenc_instance_count = 1;
    s.nvdec_instance_count = 1;
    s.pva_clock_ghz = 0.0;  // TBD
    s.nvenc_clock_max_ghz = 1.69;
    s.nvdec_clock_max_ghz = 1.69;

    // I/O
    s.pcie_version = 5;
    s.pcie_max_lanes = 0;  // not specified

    return s;
}

std::optional<SpecReference> get_spec_reference(const std::string& model) {
    if (model == "T5000" || model == "t5000") {
        return make_t5000();
    }
    if (model == "T4000" || model == "t4000") {
        return make_t4000();
    }
    return std::nullopt;
}

std::optional<SpecReference> get_spec_t5000() {
    return make_t5000();
}

std::optional<SpecReference> get_spec_t4000() {
    return make_t4000();
}

} // namespace deusridet::probe
