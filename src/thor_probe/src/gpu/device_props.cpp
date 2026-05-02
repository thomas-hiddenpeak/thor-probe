#include "gpu/device_props.h"
#include "communis/log.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>
#include <tuple>
#include <cmath>
#include <cstring>
#include <optional>
#include <cuda_runtime.h>

namespace deusridet::probe {

/* ============================================================================
   Attribute name mapping (147 attributes, CUDA 13.0)
   Canonical names from CUDA documentation; header enums are obfuscated.
   ============================================================================ */

static std::unordered_map<int, std::string> g_attr_names;
static std::once_flag g_attr_names_init;

static void init_attr_names() {
    std::call_once(g_attr_names_init, []() {
        using P = std::pair<int, std::string>;
        std::vector<P> names = {
        // Block / thread limits (1-14)
        {1,  "maxThreadsPerBlock"},
        {2,  "maxThreadsPerMultiProcessor"},
        {3,  "clockRate"},
        {4,  "instructionSchedule"},
        {5,  "computeMode"},
        {6,  "deviceOverlap"},
        {7,  "deviceSuitable"},
        {8,  "multiProcessorCount"},
        {9,  "kernelExecTimeoutEnabled"},
        {10, "Integrated"},
        {11, "canMapHostMemory"},
        {12, "computeMode"},
        {13, "concurrentKernels"},
        {14, "ECCEnabled"},

        // PCI
        {15,  "pciBusId"},
        {16,  "pciDeviceId"},
        {17,  "pciDomainID"},

        // Memory
        {18, "asyncEngineCount"},
        {19, "unifiedAddressing"},
        {20, "managedMemory"},
        {21, "singleToDoublePrecisionPerfRatio"},
        {22, " pageableMemoryAccess"},
        {23, "concurrentManagedAccess"},
        {24, "computePreemptionSupported"},
        {25, "canUseStreamWaitValue"},
        {26, "canUseStreamWaitValueNs"},
        {27, "canUseStreamWaitValueFlush"},
        {28, "reservedSharedMemoryPerBlock"},

        // Texture limits
        {29,  "hostRegisterSupported"},
        {30,  "hostNumericAtomicSupported"},
        {31,  "globalL1CacheSupported"},
        {32,  "localL1CacheSupported"},
        {33,  "sharedMemPerMultiprocessor"},
        {34,  "sharedMemPerBlock"},
        {35,  "totalConstantMemory"},
        {36,  "maxTexture1DWidth"},
        {37,  "maxTexture2DWidth"},
        {38,  "maxTexture2DHeight"},
        {39,  "maxTexture3DWidth"},
        {40,  "maxTexture3DHeight"},
        {41,  "maxTexture3DDepth"},
        {42,  "maxTexture2DArrayWidth"},
        {43,  "maxTexture2DArrayHeight"},
        {44,  "maxTexture2DArrayNumslices"},
        {45,  "surfaceAlignment"},
        {46,  "concurrentCopy"},
        {47,  "concurrentCopyAndKernel"},
        {48,  "hostRegisterSupported"},
        {49,  "pageableMemoryAccessUsesHostPageTables"},
        {50,  "directManagedMemAccessFromHost"},

        // Kernel launch
        {51,  "virtualMemoryManagementSupported"},
        {52,  "handleTypePosixIpc"},
        {53,  "handleTypeWin32"},
        {54,  "handleTypeWin32Kmt"},
        {55,  "maxBlocksPerMultiProcessor"},

        // Stream memory operations
        {56,  "streamPrioritiesSupported"},
        {57,  "globalInt32BaseAtomicsSupported"},
        {58,  "globalInt32BaseAtomicOptionOfWeak"},
        {59,  "globalInt32BaseAtomicOptionOfAcqRel"},
        {60,  "sharedInt32BaseAtomicsSupported"},
        {61,  "sharedInt32BaseAtomicOptionOfWeak"},
        {62,  "sharedInt32BaseAtomicOptionOfAcqRel"},
        {63,  "sharedFloatAtomicCasSupported"},
        {64,  "sharedFloatAtomicAddSupported"},
        {65,  "sharedInt64AtomicCasSupported"},
        {66,  "sharedInt64AtomicAddSupported"},
        {67,  "globalFloatAtomicAddSupported"},
        {68,  "globalFloatAtomicCasSupported"},
        {69,  "globalMemoryAtomicCasCapabilities"},

        // Cooperative
        {70,  "cooperativeLaunch"},
        {71,  "cooperativeMultiDeviceLaunch"},

        // Cache
        {72,  "canUseStreamWaitValue"},

        // Compute capability
        {75, "computeCapabilityMajor"},
        {76, "computeCapabilityMinor"},

        // Memory capacity
        {77, "maxSharedMemoryPerBlockOptin"},
        {78, "isMultiGpuBoard"},
        {79, "mergedSharedMemoryPerMultiprocessor"},
        {80, "hostNativeAtomicSupported"},

        // Memory pool
        {81,  "registerLimit"},
        {82,  "sharedMemPerBlockOptin"},
        {83,  "pageMigrationSupport"},

        // Memory copy
        {84,  "dedicatedCopyEngineCount"},

        // Atomic
        {85, "cudaDepth"},
        {86, "globalInt64BaseAtomicsSupported"},
        {87, "sharedFloatAtomicAddSupported"},
        {88, "sharedFloatAtomicMaxSupported"},
        {89, "globalFloatAtomicMaxSupported"},
        {90, "maxPersistentL2CacheSize"},

        // Graph
        {91,  "hostRegisterReadOnly"},
        {92,  "canUseStreamMemOps"},

        // Advanced
        {93,  "canUseStreamWaitValue"},
        {94,  "canUseStreamWaitValueNs"},
        {95,  "canUseStreamWaitValueFlush"},

        // Warp
        {96,  "fullGpu"},
        {97,  "rawECCError"},

        // Multi-gpu
        {98,  "maxTexture1DLayered"},
        {99,  "maxTexture2DGatherLayered"},
        {100, "reservedSharedMemoryPerBlock"},
        {101, "accessPolicyMaxWindowSize"},
        {102, "accessPolicyMaxHitDataSize"},
        {103, "accessPolicyMaxHitRatio"},
        {104, "accessPolicyMaxLineSize"},

        // Host
        {105, "maxTextureCubemapWidth"},
        {106, "maxTextureCubemapLayeredWidth"},
        {107, "maxTextureCubemapLayeredDepth"},
        {108, "maxTexture2DMultisample"},
        {109, "maxTexture2DMultisampleDepth"},

        // Atomic capabilities
        {110, "l2CacheMaxLineSize"},
        {111, "dirtiedPageSizeSupported"},
        {112, "virtualMemoryManagementSupported"},

        // Clock
        {113, "gpuDirectRDMASupported"},
        {114, "gpuDirectRDMAWriteWarpExecutionAlignment"},
        {115, "gpuDirectRDMAWriteBufferable"},
        {116, "memPoolSupported"},

        // Cluster / Fabric / Misc
        {117, "canUseHostRegisteredForMemOp"},
        {118, "deferredMappingSupported"},
        {119, "useHostRegisterForMemOp"},
        {120, "clusterLaunch"},
        {121, "deferredMappingCudaArraySupported"},
        {122, "canUse64BitStreamMemOps"},
        {123, "canUseStreamWaitValueNor"},
        {124, "dmaBufSupported"},
        {125, "ipcEventSupported"},
        {126, "memSyncDomainCount"},
        {127, "tensorMapAccessSupported"},
        {128, "handleTypeFabricSupported"},
        {129, "unifiedFunctionPointers"},
        {130, "numaConfig"},
        {131, "numaId"},
        {132, "multicastSupported"},

        // Gap 133-138

        {139, "gpuPciDeviceId"},
        {140, "gpuPciSubsystemId"},

        // Gap 141-147
        {141, "reserved_141"},
        {142, "reserved_142"},
        {143, "reserved_143"},
        {144, "reserved_144"},
        {145, "reserved_145"},
        {146, "reserved_146"},
        {147, "reserved_147"},
    };

    for (auto& [id, name] : names) {
        g_attr_names[id] = name;
    }
    });
}

/* ============================================================================
   Device Properties (CUDA Runtime API)
   ============================================================================ */

GpuDeviceProps query_device_props(int device) {
    GpuDeviceProps props;

    cudaDeviceProp prop;
    cudaError_t err = cudaGetDeviceProperties(&prop, device);
    if (err != cudaSuccess) {
        LOG_ERROR("ThorProbe", "cudaGetDeviceProperties failed: %s", cudaGetErrorString(err));
        return props;
    }

    props.name = prop.name;

    // Compute capability
    props.compute_major = prop.major;
    props.compute_minor = prop.minor;

    // Memory
    props.total_global_mem = prop.totalGlobalMem;
    props.shared_mem_per_sm = prop.sharedMemPerMultiprocessor;
    props.regs_per_sm = prop.regsPerMultiprocessor;

    // SM
    props.sm_count = prop.multiProcessorCount;

    // Warp
    props.warp_size = prop.warpSize;
    props.max_threads_per_block = prop.maxThreadsPerBlock;
    props.max_threads_per_sm = prop.maxThreadsPerMultiProcessor;

    // L2
    props.l2_cache_size = prop.l2CacheSize;

    // Texture limits
    props.max_texture_1d = prop.maxTexture1D;
    props.max_texture_2d[0] = prop.maxTexture2D[0];
    props.max_texture_2d[1] = prop.maxTexture2D[1];

    // Feature flags from cudaDeviceProp
    props.concurrent_kernels = prop.concurrentKernels;
    props.integrated = prop.integrated;
    props.can_map_host_memory = prop.canMapHostMemory;
    props.cooperative_launch = prop.cooperativeLaunch;
    props.memory_pool_supported = prop.memoryPoolsSupported;

    // Enumerate all attributes (1..147)
    init_attr_names();
    props.attributes.reserve(147);
    for (int id = 1; id <= 147; id++) {
        int value = 0;
        err = cudaDeviceGetAttribute(&value, (cudaDeviceAttr)id, device);
        if (err == cudaSuccess) {
            std::string name = "unknown";
            auto it = g_attr_names.find(id);
            if (it != g_attr_names.end()) name = it->second;
            props.attributes.push_back({id, name, value});
        }
    }

    // Extract clock and copy-engine info from queried attributes
    for (auto& a : props.attributes) {
        switch (a.id) {
            case 3:  props.clock_rate_khz = a.value; break;
            case 18: props.general_copy_engines = a.value; break;
            case 84: props.dedicated_copy_engines = a.value; break;
            default: break;
        }
    }
    if (props.clock_rate_max_khz == 0)
        props.clock_rate_max_khz = props.clock_rate_khz;

    // Override clock from sysfs (CUDA runtime returns stale value on embedded)
    {
        auto read_sysfs_hz = [](const std::string& path) -> long long {
            std::ifstream ifs(path);
            if (!ifs.is_open()) return 0;
            long long val = 0;
            ifs >> val;
            return val;
        };
        long long cur_hz = read_sysfs_hz("/sys/class/devfreq/gpu-gpc-0/cur_freq");
        long long max_hz = read_sysfs_hz("/sys/class/devfreq/gpu-gpc-0/max_freq");
        if (cur_hz > 0) {
            props.clock_rate_khz = static_cast<int>(cur_hz / 1000);
        }
        if (max_hz > 0) {
            props.clock_rate_max_khz = static_cast<int>(max_hz / 1000);
        }
    }

    if (props.compute_major == 11 && props.compute_minor == 0) {
        props.cuda_cores_per_sm = 128;
        props.fp32_units_per_sm = 128;
        props.fp64_units_per_sm = 2;
        props.int32_units_per_sm = 128;
        props.sfu_units_per_sm = 4;
        props.warp_schedulers_per_sm = 16;
        props.texture_units_per_sm = 4;
        props.l1_cache_size_per_sm = 256 * 1024;
        props.sp_units_per_sm = 128;
        props.dp_units_per_sm = 2;
        props.sasp_units_per_sm = 128;
        props.smem_l1_ratio = static_cast<int>(props.shared_mem_per_sm / 1024);
    }

    return props;
}

GpuDeviceProps refine_with_deep_sm(GpuDeviceProps props, std::optional<DeepSmResult> deepSm) {
    if (!deepSm) return props;

    auto& ds = *deepSm;

    if (props.compute_major == 11 && props.compute_minor == 0) {
        if (ds.warp_schedulers_per_sm.source != ProbeSource::NONE) {
            props.warp_schedulers_per_sm = ds.warp_schedulers_per_sm.value;
            LOG_INFO("ThorProbe", "Overriding warp_schedulers_per_sm: %d (source: %s)",
                     ds.warp_schedulers_per_sm.value, probe_source_name(ds.warp_schedulers_per_sm.source));
        }
        if (ds.l1_cache_size_per_sm.source != ProbeSource::NONE) {
            props.l1_cache_size_per_sm = static_cast<int>(ds.l1_cache_size_per_sm.value);
            LOG_INFO("ThorProbe", "Overriding l1_cache_size_per_sm: %zu bytes (source: %s)",
                     ds.l1_cache_size_per_sm.value, probe_source_name(ds.l1_cache_size_per_sm.source));
        }
        if (ds.smem_banks.source != ProbeSource::NONE) {
            LOG_INFO("ThorProbe", "Detected shared memory banks: %d (source: %s)",
                     ds.smem_banks.value, probe_source_name(ds.smem_banks.source));
        }
        if (ds.max_shared_per_block.source != ProbeSource::NONE) {
            LOG_INFO("ThorProbe", "Verified max_shared_per_block: %d (source: %s)",
                     ds.max_shared_per_block.value, probe_source_name(ds.max_shared_per_block.source));
        }
        if (ds.max_regs_per_thread.source != ProbeSource::NONE) {
            LOG_INFO("ThorProbe", "Verified max_regs_per_thread: %d (source: %s)",
                     ds.max_regs_per_thread.value, probe_source_name(ds.max_regs_per_thread.source));
        }
    }

    return props;
}

} // namespace deusridet::probe
