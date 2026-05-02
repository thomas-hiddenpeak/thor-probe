/**
 * @file probe_schema.h
 * @brief Unified data structures for thor_probe and thor_bench.
 *
 * Replaces the monolithic thor_probe.h. All probe/bench result types
 * live here. Individual modules reference this schema rather than
 * duplicating types.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace deusridet::probe {

/* ============================================================================
 * 1. GPU Device Properties (from CUDA Runtime API)
 * ============================================================================ */

struct AttributeEntry {
    int id;
    std::string name;
    int value;
};

struct GpuDeviceProps {
    std::string         name;
    int                 compute_major = 0;
    int                 compute_minor = 0;
    size_t              total_global_mem = 0;
    size_t              shared_mem_per_sm = 0;
    int                 regs_per_sm = 0;
    int                 sm_count = 0;
    int                 warp_size = 32;
    int                 max_threads_per_block = 1024;
    int                 max_threads_per_sm = 0;
    size_t              l2_cache_size = 0;
    int                 max_texture_1d = 0;
    int                 max_texture_2d[2] = {0, 0};
    bool                concurrent_kernels = false;
    bool                integrated = false;
    bool                can_map_host_memory = false;
    bool                cooperative_launch = false;
    bool                memory_pool_supported = false;
    std::vector<AttributeEntry> attributes;

    // SM microarchitecture detail (derived from cudaDeviceAttr + arch-specific constants)
    int                 cuda_cores_per_sm = 0;
    int                 fp32_units_per_sm = 0;
    int                 fp64_units_per_sm = 0;
    int                 int32_units_per_sm = 0;
    int                 int8_units_per_sm = 0;
    int                 sfu_units_per_sm = 0;
    int                 warp_schedulers_per_sm = 0;
    int                 texture_units_per_sm = 0;
    int                 l1_cache_size_per_sm = 0;   // in bytes
    int                 sp_units_per_sm = 0;         // scalar processors = CUDA cores
    int                 dp_units_per_sm = 0;         // double precision units
    int                 sasp_units_per_sm = 0;       // Systolic Array SP (for tcgen05)
    int                 smem_l1_ratio = 0;           // shared mem size in KB relative to L1

    // Clock info
    int                 clock_rate_khz = 0;
    int                 clock_rate_max_khz = 0;

    // Copy engines
    int                 dedicated_copy_engines = 0;
    int                 general_copy_engines = 0;

    int total_warps_per_sm() const { return max_threads_per_sm / warp_size; }
    double global_mem_gb() const { return total_global_mem / (1024.0 * 1024.0 * 1024.0); }
};

/* ============================================================================
 * 2. Tensor Core (tcgen05) Capability Detection
 * ============================================================================ */

enum class MmaType : uint8_t {
    NVFP4_BLOCK_SCALE,
    FP8_E4M3,
    FP8_E5M2,
    FP16,
    BF16,
    TF32,
    INT8,
    INT4,
    COUNT
};

struct MmaCapability {
    bool supported = false;
    int  m_size = 0;
    int  n_size = 0;
    int  k_size = 0;
    std::string note;
};

struct BarrierCapability {
    bool cluster_launch = false;
    bool cluster_width_supported = false;
    bool cluster_height_supported = false;
    bool cluster_depth_supported = false;
    bool grid_mem_fence_supported = false;
    bool cluster_mem_fence_supported = false;
    int  max_cluster_width = 0;
    int  max_cluster_height = 0;
    int  max_cluster_depth = 0;
};

struct TmemCapability {
    bool supported = false;
    size_t total_bytes = 0;
    bool cp_async_tmem = false;
    bool mma_tmem = false;
};

struct AsyncCopyCapability {
    bool tcgen05_cp = false;
    bool shared_mem_fence = false;
    bool barrier_notify = false;
};

struct WarpCapability {
    bool shuffle_supported = false;
    bool vote_supported = false;
};

struct TcGen05Capability {
    MmaCapability mma[static_cast<size_t>(MmaType::COUNT)];
    TmemCapability tmem;
    AsyncCopyCapability async_copy;
    BarrierCapability barrier;
    WarpCapability warp;

    bool has_nvfp4() const { return mma[static_cast<size_t>(MmaType::NVFP4_BLOCK_SCALE)].supported; }
    bool has_fp8() const { return mma[static_cast<size_t>(MmaType::FP8_E4M3)].supported; }
};

/* ============================================================================
 * 3. Source Attribution
 * ============================================================================ */

enum class ProbeSource : uint8_t {
    NONE = 0,
    cuda_api,        // cudaDeviceGetAttribute / cudaDeviceProp
    sysfs,           // /sys/class/devfreq/, /sys/devices/system/cpu/, etc.
    procfs,          // /proc/cpuinfo, /proc/meminfo
    dynamic_probe,   // CUDA kernel-based measurement (occupancy, IPC, cache probe, etc.)
    cc_lookup,       // compute capability table lookup (reference data)
    benchmark_ratio, // derived from benchmark throughput ratios
    hardcoded_spec   // from NVIDIA documentation / spec sheets
};

inline const char* probe_source_name(ProbeSource s) {
    switch (s) {
        case ProbeSource::cuda_api:       return "cuda_api";
        case ProbeSource::sysfs:          return "sysfs";
        case ProbeSource::procfs:         return "procfs";
        case ProbeSource::dynamic_probe:  return "dynamic_probe";
        case ProbeSource::cc_lookup:      return "cc_lookup";
        case ProbeSource::benchmark_ratio: return "benchmark_ratio";
        case ProbeSource::hardcoded_spec: return "hardcoded_spec";
        default: return "unknown";
    }
}

/* ============================================================================
 * 3b. Sourced Value — wraps any int/double with provenance
 * ============================================================================ */

template<typename T>
struct SourcedValue {
    T value = 0;
    ProbeSource source = ProbeSource::NONE;
    std::string note;

    explicit SourcedValue(T v = 0, ProbeSource s = ProbeSource::NONE, std::string n = "")
        : value(v), source(s), note(std::move(n)) {}
    operator T() const { return value; }
    bool valid() const { return source != ProbeSource::NONE; }
    const char* sourceStr() const { return probe_source_name(source); }
};

using IntSourced   = SourcedValue<int>;
using DoubleSourced = SourcedValue<double>;
using SizeSourced  = SourcedValue<size_t>;

/* ============================================================================
 * 3c. Deep SM Microarchitecture Results (dynamically probed)
 * ============================================================================ */

struct DeepSmResult {
    // --- Warp schedulers ---
    IntSourced   warp_schedulers_per_sm;        // from IPC saturation probe

    // --- Shared memory banks ---
    IntSourced   smem_banks;                    // from bank conflict probe
    IntSourced   smem_bank_width_bits;          // from bank conflict probe

    // --- L1 cache ---
    SizeSourced  l1_cache_size_per_sm;          // from cache probe (inflection point)

    // --- Occupancy-derived ---
    IntSourced   max_regs_per_thread;           // from occupancy curve
    IntSourced   max_shared_per_block;          // from occupancy curve

    // --- Theoretical limits (from CUDA API, verified) ---
    IntSourced   max_registers_per_sm;          // cudaDeviceProp::regsPerMultiprocessor
    IntSourced   max_shared_per_sm;             // cudaDeviceProp::sharedMemPerMultiprocessor
    IntSourced   max_threads_per_sm;            // cudaDeviceProp::maxThreadsPerMultiProcessor
};

/* ============================================================================
 * 4. GPU Result (device + capabilities)
 * ============================================================================ */

struct GpuResult {
    GpuDeviceProps device;
    std::optional<TcGen05Capability> tcgen05;
    std::optional<DeepSmResult> deep_sm;
};

/* ============================================================================
 * 5. Multimedia Results
 * ============================================================================ */

struct NvencCaps {
    std::string status = "available";
    int instance_count = 0;
    bool h264_encode = false;
    bool hevc_encode = false;
    bool av1_encode = false;
    unsigned int clock_mhz = 0;
    std::vector<std::string> supported_presets;
    int max_bitrate_mbps = 0;
};

struct NvdecCaps {
    std::string status = "available";
    int instance_count = 0;
    std::vector<std::string> supported_codecs;
    unsigned int clock_mhz = 0;
    struct ResolutionLimit { int width = 0, height = 0; };
    std::vector<ResolutionLimit> limits;
};

struct PvaInfo {
    std::string status = "available";
    uint32_t engine_count = 0;
    uint32_t vpu_count = 0;
    std::string generation;
    unsigned int clock_mhz = 0;
    uint32_t driver_version = 0;
    uint32_t runtime_version = 0;
    double int8_gmac_s = 0;
    double fp16_gflops_s = 0;
};

struct OfaInfo {
    std::string status = "available";
    bool available = false;
    unsigned int clock_mhz = 0;
    bool supports_optical_flow = false;
    bool supports_stereo_disparity = false;
};

struct GenericProbeComponent {
    std::string status = "available";
    unsigned int clock_mhz = 0;
};

struct MultimediaResult {
    NvencCaps nvenc;
    NvdecCaps nvdec;
    GenericProbeComponent nvjpeg;
    PvaInfo pva;
    OfaInfo ofa;
    GenericProbeComponent isp[2];
    GenericProbeComponent vic;
};

/* ============================================================================
 * 6. CPU Results
 * ============================================================================ */

struct CpuCoreInfo {
    int core_id = 0;
    std::string features;
    unsigned int mhz = 0;
    unsigned int max_mhz = 0;
    unsigned int min_mhz = 0;
    unsigned int bogomips = 0;
    int physical_id = 0;
};

struct CpuResult {
    std::string architecture;
    std::string model_name;
    std::string cpu_part;
    std::string cpu_implementer;
    std::string platform;
    int core_count = 0;
    int physical_id_count = 0;
    unsigned int cpu_min_mhz = 0;
    unsigned int cpu_max_mhz = 0;
    std::vector<CpuCoreInfo> cores;
    struct CacheInfo {
        size_t l1d_per_core_kb = 0;
        size_t l1i_per_core_kb = 0;
        size_t l2_per_cluster_kb = 0;
        size_t l3_total_kb = 0;
    } cache;
};

/* ============================================================================
 * 7. System Results
 * ============================================================================ */

struct NetworkInterface {
    std::string name;
    std::string driver;
    std::string speed;
};

struct PciePort {
    std::string name;
    int link_speed = 0;
    int link_width = 0;
};

struct SystemResult {
    struct {
        std::string type;
        int bus_width_bits = 256;
        double peak_bandwidth_gb_s = 273.0;
        size_t total_bytes = 0;
    } memory;
    struct {
        std::vector<std::string> outputs;
        int output_count = 0;
    } display;
    struct {
        std::vector<NetworkInterface> interfaces;
    } network;
    struct {
        std::vector<PciePort> ports;
        std::string version;
    } pcie;
};

/* ============================================================================
 * 8. Telemetry Snapshot (replaces NvmlState — sysfs + tegrastats only)
 * ============================================================================ */

struct TelemetrySnapshot {
    double timestamp_s = 0;
    struct {
        unsigned int gpu_mhz = 0;
        unsigned int emc_mhz = 0;
        unsigned int nvenc0_mhz = 0, nvenc1_mhz = 0;
        unsigned int nvdec0_mhz = 0, nvdec1_mhz = 0;
        unsigned int nvjpg_mhz = 0;
        unsigned int vic_mhz = 0;
        unsigned int ofa_mhz = 0;
        unsigned int pva_mhz = 0;
        unsigned int ape_mhz = 0;
        unsigned int cpu_mhz = 0;
    } clocks;
    struct {
        unsigned int gpu_temp_c = 0;
        unsigned int soc_temp_c = 0;
        std::vector<std::pair<std::string, unsigned int>> all_zones;
    } thermal;
    struct {
        unsigned int gpu_mw = 0;
        unsigned int cpu_soc_mw = 0;
        unsigned int vin_sys_mw = 0;
        unsigned int emc_bw_mbps = 0;
    } power;
    unsigned int ram_used_mb = 0, ram_total_mb = 0;
    std::string source;
};

/* ============================================================================
 * 9. Benchmark Results (from micro-benchmarks)
 * ============================================================================ */

struct BenchResult {
    // --- Memory bandwidth ---
    double global_mem_read_peak_bw_gb_s = 0;
    double global_mem_write_peak_bw_gb_s = 0;
    double global_mem_read_scalar_bw_gb_s = 0;
    double shared_mem_bw_gb_s = 0;
    double device_to_host_bw_gb_s = 0;
    double host_to_device_bw_gb_s = 0;

    // --- Scalar compute ---
    std::optional<double> scalar_fp32_tops;
    std::optional<double> scalar_fp16_tops;
    std::optional<double> scalar_fp64_tops;
    std::optional<double> scalar_int32_tops;
    std::optional<double> sfu_sin_tops;
    std::optional<double> sfu_exp_tops;

    // --- Tensor Core compute ---
    std::optional<double> tc_fp16_tops;
    std::optional<double> tc_bf16_tops;
    std::optional<double> tc_fp8_tops;
    std::optional<double> tc_nvfp4_tops;
    std::optional<double> tc_int8_tops;

    // --- Theoretical peak (for reference ratio) ---
    std::optional<double> scalar_fp32_theoretical_tops;
    std::optional<double> scalar_fp16_theoretical_tops;
    std::optional<double> scalar_fp64_theoretical_tops;
    std::optional<double> scalar_int32_theoretical_tops;
    std::optional<double> tc_fp16_theoretical_tops;
    std::optional<double> tc_bf16_theoretical_tops;
    std::optional<double> tc_fp8_theoretical_tops;
    std::optional<double> tc_nvfp4_theoretical_tops;
    std::optional<double> tc_int8_theoretical_tops;

    // --- SM110a/Blackwell-specific features ---
    std::optional<double> tmem_bw_gb_s;
    std::optional<double> cluster_fence_rate;

    // --- Isolated memory layer bandwidth ---
    std::optional<double> l2_read_bw_gb_s;
    std::optional<double> l1_read_bw_gb_s;
    std::optional<double> cp_async_bw_gb_s;
};

/* ============================================================================
 * 10. Full Probe Result (top-level output)
 * ============================================================================ */

struct FullProbeResult {
    std::string platform = "DeusRidet-Thor";
    std::string version;
    double timestamp_s = 0;

    GpuResult gpu;
    std::optional<MultimediaResult> multimedia;
    std::optional<CpuResult> cpu;
    std::optional<SystemResult> system;
    std::optional<TelemetrySnapshot> telemetry;
    std::optional<BenchResult> benchmarks;
};

} // namespace deusridet::probe
