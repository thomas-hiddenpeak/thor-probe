#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "probe_schema.h"
#include "gpu/device_props.h"
#include "gpu/tc_probe.h"
#include "gpu/deep_sm_probe.h"
#include "cpu/cpu_probe.h"
#include "multimedia/nvenc.h"
#include "multimedia/nvdec.h"
#include "multimedia/nvjpeg.h"
#include "multimedia/pva.h"
#include "multimedia/ofa.h"
#include "multimedia/isp.h"
#include "multimedia/vic.h"
#include "system/memory.h"
#include "system/display.h"
#include "system/network.h"
#include "system/pcie.h"
#include "telemetry/telemetry_manager.h"
#include "communis/cuda_check.h"

using namespace deusridet::probe;

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [OPTIONS]\n"
              << "  DEVICE    CUDA device index (default: 0)\n"
              << "  --json    Output as JSON instead of text\n"
              << "  --help    Show this message\n";
}

static void print_section_header(const std::string& title) {
    constexpr int W = 72;
    std::cout << std::endl;
    std::cout << std::string(W, '=') << std::endl;
    std::cout << title << std::endl;
    std::cout << std::string(W, '=') << std::endl;
}

static void print_field(const std::string& label, const std::string& value, int width = 28) {
    std::cout << "  " << std::left << std::setw(width) << label << value << std::endl;
}

static void print_field(const std::string& label, bool value, int width = 28) {
    print_field(label, std::string(value ? "yes" : "no"), width);
}

static void print_field(const std::string& label, int value, int width = 28) {
    print_field(label, std::to_string(value), width);
}

static void print_field(const std::string& label, double value, int width = 28) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << value;
    print_field(label, oss.str(), width);
}

static void print_gpu_result(const GpuResult& gpu) {
    print_section_header("GPU Device Properties");
    auto& d = gpu.device;
    print_field("Name", d.name);
    print_field("Compute Capability", std::to_string(d.compute_major) + "." + std::to_string(d.compute_minor));
    print_field("SM Count", d.sm_count);
    print_field("Global Memory", std::to_string(static_cast<int>(d.global_mem_gb())) + " GB");
    print_field("Shared Mem/SM", std::to_string(static_cast<int>(d.shared_mem_per_sm / 1024)) + " KB");
    print_field("Registers/SM", d.regs_per_sm);
    print_field("Warp Size", d.warp_size);
    print_field("Max Threads/Block", d.max_threads_per_block);
    print_field("Max Threads/SM", d.max_threads_per_sm);
    print_field("Total Warps/SM", d.total_warps_per_sm());
    print_field("L2 Cache", std::to_string(static_cast<int>(d.l2_cache_size / (1024*1024))) + " MB");
    print_field("Clock Rate (kHz)", d.clock_rate_khz);
    print_field("Max Clock Rate (kHz)", d.clock_rate_max_khz);
    print_field("Concurrent Kernels", d.concurrent_kernels);
    print_field("Integrated", d.integrated);
    print_field("Memory Pool", d.memory_pool_supported);
    print_field("Cooperative Launch", d.cooperative_launch);

    if (d.cuda_cores_per_sm > 0) {
        print_field("CUDA Cores/SM", d.cuda_cores_per_sm);
        print_field("FP32 Units/SM", d.fp32_units_per_sm);
        print_field("FP64 Units/SM", d.fp64_units_per_sm);
        print_field("INT32 Units/SM", d.int32_units_per_sm);
        print_field("INT8 Units/SM", d.int8_units_per_sm);
        print_field("SASP Units/SM", d.sasp_units_per_sm);
        print_field("SFU Units/SM", d.sfu_units_per_sm);
        print_field("Warp Schedulers/SM", d.warp_schedulers_per_sm);
        print_field("Texture Units/SM", d.texture_units_per_sm);
        print_field("L1 Cache/SM", std::to_string(d.l1_cache_size_per_sm) + " bytes");
    }

    if (gpu.tcgen05) {
        print_section_header("tcgen05 Tensor Core Capabilities");
        auto& tc = *gpu.tcgen05;

        static const char* mma_names[] = {
            "nvFP4 (Block Scale)", "FP8 E4M3", "FP8 E5M2", "FP16",
            "BF16", "TF32", "INT8", "INT4"
        };
        for (size_t i = 0; i < static_cast<size_t>(MmaType::COUNT); ++i) {
            auto& m = tc.mma[i];
            if (m.supported) {
                std::cout << "  " << std::left << std::setw(28)
                          << mma_names[i]
                          << "M" << m.m_size << "N" << m.n_size << "K" << m.k_size;
                if (!m.note.empty()) std::cout << " (" << m.note << ")";
                std::cout << std::endl;
            }
        }

        print_field("Tmem Supported", tc.tmem.supported);
        if (tc.tmem.supported)
            print_field("Tmem Total", std::to_string(static_cast<int>(tc.tmem.total_bytes / 1024)) + " KB");
        print_field("Async Copy TC", tc.async_copy.tcgen05_cp);
        print_field("Cluster Launch", tc.barrier.cluster_launch);
        print_field("Grid Mem Fence", tc.barrier.grid_mem_fence_supported);
        print_field("Cluster Mem Fence", tc.barrier.cluster_mem_fence_supported);
        print_field("Shuffle", tc.warp.shuffle_supported);
        print_field("Vote", tc.warp.vote_supported);
    }

    if (gpu.deep_sm) {
        print_section_header("Deep SM Microarchitecture (Dynamic Probes)");
        auto& sm = *gpu.deep_sm;

        auto print_sourced = [](const std::string& label, const IntSourced& val, int width = 32) {
            if (val.value > 0) {
                std::string detail = std::to_string(val.value);
                if (!val.note.empty()) detail += " (" + val.note + ")";
                print_field(label, detail, width);
            }
        };
        auto print_sourced_size = [](const std::string& label, const SizeSourced& val, int width = 32) {
            if (val.value > 0) {
                std::string detail = std::to_string(val.value / 1024) + " KB";
                if (!val.note.empty()) detail += " (" + val.note + ")";
                print_field(label, detail, width);
            }
        };

        print_sourced("Warp Schedulers/SM", sm.warp_schedulers_per_sm);
        print_sourced("Smem Banks", sm.smem_banks);
        if (sm.smem_bank_width_bits.value > 0) print_field("Smem Bank Width", std::to_string(sm.smem_bank_width_bits.value) + " bits");
        print_sourced_size("L1 Cache/SM", sm.l1_cache_size_per_sm);
        print_sourced("Max Regs/Thread", sm.max_regs_per_thread);
        if (sm.max_shared_per_block.value > 0) print_field("Max Shared/Block", std::to_string(sm.max_shared_per_block.value / 1024) + " KB");
        print_sourced("Max Registers/SM", sm.max_registers_per_sm);
        if (sm.max_shared_per_sm.value > 0) print_field("Max Shared/SM", std::to_string(sm.max_shared_per_sm.value / 1024) + " KB");
        print_sourced("Max Threads/SM", sm.max_threads_per_sm);
    }
}

static void print_cpu_result(const CpuResult& cpu) {
    print_section_header("CPU");
    print_field("Architecture", cpu.architecture);
    print_field("Model", cpu.model_name);
    print_field("Core Count", cpu.core_count);
    print_field("Physical IDs", cpu.physical_id_count);
    print_field("L1d/Core", std::to_string(cpu.cache.l1d_per_core_kb) + " KB");
    print_field("L1i/Core", std::to_string(cpu.cache.l1i_per_core_kb) + " KB");
    print_field("L2/Cluster", std::to_string(cpu.cache.l2_per_cluster_kb) + " KB");
    print_field("L3 Total", std::to_string(cpu.cache.l3_total_kb) + " KB");
}

static void print_multimedia_result(const MultimediaResult& mm) {
    print_section_header("Multimedia");
    print_field("NVENC Status", mm.nvenc.status);
    print_field("NVENC H.264", mm.nvenc.h264_encode);
    print_field("NVENC HEVC", mm.nvenc.hevc_encode);
    print_field("NVENC AV1", mm.nvenc.av1_encode);
    print_field("NVENC Clock", std::to_string(mm.nvenc.clock_mhz) + " MHz");

    print_field("NVDEC Status", mm.nvdec.status);
    print_field("NVDEC Clock", std::to_string(mm.nvdec.clock_mhz) + " MHz");
    print_field("NVDEC Codecs", "");
    for (auto& c : mm.nvdec.supported_codecs)
        std::cout << "    - " << c << std::endl;

    print_field("NVJPEG Status", mm.nvjpeg.status);
    print_field("PVA Status", mm.pva.status);
    print_field("PVA Clock", std::to_string(mm.pva.clock_mhz) + " MHz");
    print_field("PVA INT8 GMAC/s", mm.pva.int8_gmac_s);
    print_field("PVA FP16 GFLOPS/s", mm.pva.fp16_gflops_s);

    print_field("OFA Status", mm.ofa.status);
    print_field("OFA Optical Flow", mm.ofa.supports_optical_flow);
    print_field("OFA Stereo", mm.ofa.supports_stereo_disparity);

    for (int i = 0; i < 2; ++i) {
        print_field("ISP" + std::to_string(i) + " Status", mm.isp[i].status);
    }
    print_field("VIC Status", mm.vic.status);
}

static void print_system_result(const SystemResult& sys) {
    print_section_header("System");
    print_field("Memory Type", sys.memory.type);
    print_field("Bus Width", std::to_string(sys.memory.bus_width_bits) + " bits");
    print_field("Peak BW", std::to_string(static_cast<int>(sys.memory.peak_bandwidth_gb_s)) + " GB/s");
    print_field("Total Memory", std::to_string(static_cast<int>(sys.memory.total_bytes / (1024*1024*1024))) + " GB");

    print_field("Display Outputs", sys.display.output_count);
    for (auto& o : sys.display.outputs)
        std::cout << "    - " << o << std::endl;

    print_field("Network Interfaces", std::to_string(static_cast<int>(sys.network.interfaces.size())));
    for (auto& n : sys.network.interfaces)
        std::cout << "    - " << n.name << " (" << n.speed << ")" << std::endl;

    print_field("PCIe Version", sys.pcie.version);
    for (auto& p : sys.pcie.ports)
        std::cout << "    - " << p.name << " (speed=" << p.link_speed << ", width=" << p.link_width << ")" << std::endl;
}

static void print_telemetry(const TelemetrySnapshot& ts) {
    print_section_header("Telemetry Snapshot");
    auto& c = ts.clocks;
    print_field("GPU Clock", std::to_string(c.gpu_mhz) + " MHz");
    print_field("EMC Clock", std::to_string(c.emc_mhz) + " MHz");
    print_field("CPU Clock", std::to_string(c.cpu_mhz) + " MHz");
    print_field("NVENC0 Clock", std::to_string(c.nvenc0_mhz) + " MHz");
    print_field("NVDEC0 Clock", std::to_string(c.nvdec0_mhz) + " MHz");
    print_field("VIC Clock", std::to_string(c.vic_mhz) + " MHz");
    print_field("PVA Clock", std::to_string(c.pva_mhz) + " MHz");
    print_field("OFA Clock", std::to_string(c.ofa_mhz) + " MHz");

    auto& t = ts.thermal;
    print_field("GPU Temp", std::to_string(t.gpu_temp_c) + " C");
    print_field("SOC Temp", std::to_string(t.soc_temp_c) + " C");

    auto& p = ts.power;
    print_field("GPU Power", std::to_string(p.gpu_mw) + " mW");
    print_field("CPU+SOC Power", std::to_string(p.cpu_soc_mw) + " mW");
    print_field("EMC BW", std::to_string(p.emc_bw_mbps) + " Mbps");
    print_field("RAM Used/Total", std::to_string(ts.ram_used_mb) + " / " + std::to_string(ts.ram_total_mb) + " MB");
    if (!ts.source.empty()) print_field("Source", ts.source);
}

static std::string json_str(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[7];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
                break;
        }
    }
    return "\"" + out + "\"";
}

static bool gpu_is_empty(const GpuResult& r) {
    return r.device.name.empty() && r.device.sm_count == 0
        && !r.tcgen05.has_value() && !r.deep_sm.has_value();
}

static void json_print_result(const FullProbeResult& result) {
    auto& out = std::cout;
    int sections = 0;
    const int totalSections = 1 +
        (result.cpu ? 1 : 0) +
        (result.multimedia ? 1 : 0) +
        (result.system ? 1 : 0) +
        (result.telemetry ? 1 : 0);

    out << "{\n";
    out << "  \"platform\": " << json_str(result.platform) << ",\n";
    out << "  \"version\": " << json_str(result.version) << ",\n";
    out << "  \"timestamp_s\": " << result.timestamp_s << ",\n";

    /* gpu section */
    if (gpu_is_empty(result.gpu)) {
        out << "  \"gpu\": null";
    } else {
        out << "  \"gpu\": {\n";
        out << "    \"device\": {\n";
        auto& d = result.gpu.device;
        out << "      \"name\": " << json_str(d.name) << ",\n";
        out << "      \"compute_capability\": \"" << d.compute_major << "." << d.compute_minor << "\",\n";
        out << "      \"sm_count\": " << d.sm_count << ",\n";
        double gb = d.global_mem_gb();
        out << "      \"global_mem_gb\": " << (std::isnan(gb) || std::isinf(gb) ? 0.0 : gb) << ",\n";
        out << "      \"shared_mem_per_sm\": " << d.shared_mem_per_sm << ",\n";
        out << "      \"regs_per_sm\": " << d.regs_per_sm << ",\n";
        out << "      \"clock_rate_khz\": " << d.clock_rate_khz << ",\n";
        out << "      \"max_clock_rate_khz\": " << d.clock_rate_max_khz << "\n";
        out << "    },\n";

        if (result.gpu.tcgen05) {
            out << "    \"tcgen05\": {\n";
            out << "      \"has_nvfp4\": " << (result.gpu.tcgen05->has_nvfp4() ? "true" : "false") << ",\n";
            out << "      \"has_fp8\": " << (result.gpu.tcgen05->has_fp8() ? "true" : "false") << ",\n";
            out << "      \"tmem_supported\": " << (result.gpu.tcgen05->tmem.supported ? "true" : "false") << "\n";
            out << "    },\n";
        } else {
            out << "    \"tcgen05\": null,\n";
        }

        if (result.gpu.deep_sm) {
            auto& sm = *result.gpu.deep_sm;
            out << "    \"deep_sm\": {\n";
            out << "      \"warp_schedulers_per_sm\": { \"value\": " << sm.warp_schedulers_per_sm.value << ", \"source\": " << json_str(sm.warp_schedulers_per_sm.sourceStr()) << " },\n";
            out << "      \"smem_banks\": { \"value\": " << sm.smem_banks.value << ", \"source\": " << json_str(sm.smem_banks.sourceStr()) << " },\n";
            out << "      \"smem_bank_width_bits\": { \"value\": " << sm.smem_bank_width_bits.value << ", \"source\": " << json_str(sm.smem_bank_width_bits.sourceStr()) << " },\n";
            out << "      \"l1_cache_size_per_sm\": { \"value\": " << sm.l1_cache_size_per_sm.value << ", \"source\": " << json_str(sm.l1_cache_size_per_sm.sourceStr()) << " },\n";
            out << "      \"max_regs_per_thread\": { \"value\": " << sm.max_regs_per_thread.value << ", \"source\": " << json_str(sm.max_regs_per_thread.sourceStr()) << " },\n";
            out << "      \"max_shared_per_block\": { \"value\": " << sm.max_shared_per_block.value << ", \"source\": " << json_str(sm.max_shared_per_block.sourceStr()) << " },\n";
            out << "      \"max_registers_per_sm\": { \"value\": " << sm.max_registers_per_sm.value << ", \"source\": " << json_str(sm.max_registers_per_sm.sourceStr()) << " },\n";
            out << "      \"max_shared_per_sm\": { \"value\": " << sm.max_shared_per_sm.value << ", \"source\": " << json_str(sm.max_shared_per_sm.sourceStr()) << " },\n";
            out << "      \"max_threads_per_sm\": { \"value\": " << sm.max_threads_per_sm.value << ", \"source\": " << json_str(sm.max_threads_per_sm.sourceStr()) << " }\n";
            out << "    }\n";
        } else {
            out << "    \"deep_sm\": null\n";
        }
        out << "  }";
    }
    sections++;
    if (sections < totalSections) out << ",";
    out << "\n";

    if (result.cpu) {
        auto& c = *result.cpu;
        out << "  \"cpu\": {\n";
        out << "    \"architecture\": " << json_str(c.architecture) << ",\n";
        out << "    \"model_name\": " << json_str(c.model_name) << ",\n";
        out << "    \"core_count\": " << c.core_count << "\n";
        out << "  }";
        sections++;
        if (sections < totalSections) out << ",";
        out << "\n";
    }

    if (result.multimedia) {
        auto& m = *result.multimedia;
        out << "  \"multimedia\": {\n";
        out << "    \"nvenc\": { \"status\": " << json_str(m.nvenc.status) << ", \"h264\": " << (m.nvenc.h264_encode ? "true" : "false") << " },\n";
        out << "    \"nvdec\": { \"status\": " << json_str(m.nvdec.status) << " },\n";
        out << "    \"pva\": { \"status\": " << json_str(m.pva.status) << ", \"clock_mhz\": " << m.pva.clock_mhz << " },\n";
        out << "    \"ofa\": { \"status\": " << json_str(m.ofa.status) << " }\n";
        out << "  }";
        sections++;
        if (sections < totalSections) out << ",";
        out << "\n";
    }

    if (result.system) {
        auto& s = *result.system;
        out << "  \"system\": {\n";
        out << "    \"memory\": { \"type\": " << json_str(s.memory.type) << ", \"total_bytes\": " << s.memory.total_bytes << " }\n";
        out << "  }";
        sections++;
        if (sections < totalSections) out << ",";
        out << "\n";
    }

    if (result.telemetry) {
        auto& t = *result.telemetry;
        out << "  \"telemetry\": {\n";
        out << "    \"gpu_mhz\": " << t.clocks.gpu_mhz << ",\n";
        out << "    \"gpu_temp_c\": " << t.thermal.gpu_temp_c << ",\n";
        out << "    \"gpu_power_mw\": " << t.power.gpu_mw << "\n";
        out << "  }";
        sections++;
        if (sections < totalSections) out << ",";
        out << "\n";
    }

    out << "}\n";
}

int main(int argc, char* argv[]) {
    int device = 0;
    bool json_out = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--json") {
            json_out = true;
        } else if (arg[0] == '-') {
            std::cerr << "Error: unrecognized option: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        } else {
            try {
                device = std::stoi(arg);
                if (device < 0) {
                    std::cerr << "Error: device ID must be non-negative: " << arg << std::endl;
                    print_usage(argv[0]);
                    return 1;
                }
            } catch (const std::invalid_argument&) {
                std::cerr << "Error: invalid device ID: " << arg << std::endl;
                print_usage(argv[0]);
                return 1;
            } catch (const std::out_of_range&) {
                std::cerr << "Error: device ID out of range: " << arg << std::endl;
                print_usage(argv[0]);
                return 1;
            }
        }
    }

    const char* banner = "DeusRidet-Thor Hardware Probe v0.2.0\n"
                         "=====================================\n";
    if (json_out) {
        std::cerr << banner;
    } else {
        std::cout << banner;
    }

    auto& out = json_out ? std::cerr : std::cout;

    FullProbeResult result;
    result.timestamp_s = static_cast<double>(std::time(nullptr));
    result.version = "0.2.0";

    /* --- GPU device validation --- */
    int deviceCount = 0;
    bool gpuAvailable = false;
    cudaError_t err = cudaGetDeviceCount(&deviceCount);
    if (err == cudaSuccess && deviceCount > 0 && device >= 0 && device < deviceCount) {
        try {
            cudaCheck(cudaSetDevice(device));
            gpuAvailable = true;
        } catch (const std::exception& e) {
            LOG_ERROR("GPU", "cudaSetDevice(%d) failed: %s", device, e.what());
        }
    } else if (err != cudaSuccess) {
        LOG_WARN("GPU", "cudaGetDeviceCount failed: %s", cudaGetErrorString(err));
    } else if (deviceCount == 0) {
        LOG_WARN("GPU", "No CUDA devices available");
    } else {
        LOG_ERROR("GPU", "Invalid device %d (only %d devices)", device, deviceCount);
    }

    if (gpuAvailable) {
        try {
            result.gpu.device = query_device_props(device);
            out << "[gpu] Probed: " << result.gpu.device.name << std::endl;

            result.gpu.tcgen05 = detect_tcgen05_capabilities(device);
            out << "[tcgen05] Capabilities detected" << std::endl;

            result.gpu.deep_sm = run_deep_sm_probe(device);
            out << "[deep_sm] Dynamic probes complete" << std::endl;

            result.gpu.device = refine_with_deep_sm(result.gpu.device, result.gpu.deep_sm);
        } catch (const std::exception& e) {
            std::cerr << "[gpu] FAILED: " << e.what() << std::endl;
            gpuAvailable = false;
            result.gpu = GpuResult{};
        } catch (...) {
            LOG_ERROR("GPU", "Unknown GPU probe failure");
            gpuAvailable = false;
            result.gpu = GpuResult{};
        }
    }


    /* --- CPU --- */
    try {
        result.cpu = probe_cpu();
        out << "[cpu] Probed: " << result.cpu->model_name << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[cpu] FAILED: " << e.what() << std::endl;
    }

    /* --- Multimedia --- */
    MultimediaResult mm;
    try { mm.nvenc = probe_nvenc(); } catch (...) { mm.nvenc.status = "error"; }
    try { mm.nvdec = probe_nvdec(); } catch (...) { mm.nvdec.status = "error"; }
    try { mm.nvjpeg = probe_nvjpeg(); } catch (...) { mm.nvjpeg.status = "error"; }
    try { mm.pva = probe_pva(); } catch (...) { mm.pva.status = "error"; }
    try { mm.ofa = probe_ofa(); } catch (...) { mm.ofa.status = "error"; }
    try { mm.isp[0] = probe_isp(0); } catch (...) { mm.isp[0].status = "error"; }
    try { mm.isp[1] = probe_isp(1); } catch (...) { mm.isp[1].status = "error"; }
    try { mm.vic = probe_vic(); } catch (...) { mm.vic.status = "error"; }
    result.multimedia = mm;
    out << "[multimedia] Probed" << std::endl;

    /* --- System --- */
    SystemResult sys;
    try { sys.memory = probe_memory(); } catch (...) { LOG_WARN("System", "probe_memory failed"); }
    try { sys.display = probe_display(); } catch (...) { LOG_WARN("System", "probe_display failed"); }
    try { sys.network = probe_network(); } catch (...) { LOG_WARN("System", "probe_network failed"); }
    try { sys.pcie = probe_pcie(); } catch (...) { LOG_WARN("System", "probe_pcie failed"); }
    result.system = sys;
    out << "[system] Probed" << std::endl;

    /* --- Telemetry --- */
    try {
        deusridet::telemetry::TelemetryManager tm;
        result.telemetry = tm.snapshot();
        out << "[telemetry] Snapshot taken" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[telemetry] FAILED: " << e.what() << std::endl;
    }

    /* --- Output --- */
    if (json_out) {
        json_print_result(result);
    } else {
        if (gpuAvailable) print_gpu_result(result.gpu);
        std::cout.flush();
        if (result.cpu) print_cpu_result(*result.cpu);
        std::cout.flush();
        if (result.multimedia) print_multimedia_result(*result.multimedia);
        std::cout.flush();
        if (result.system) print_system_result(*result.system);
        std::cout.flush();
        if (result.telemetry) print_telemetry(*result.telemetry);
        std::cout.flush();
    }

    return 0;
}
