// test_spec_reference.cpp — Unit tests for deusridet::probe::SpecReference
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "communis/spec_reference.h"

using Catch::Approx;

using namespace deusridet::probe;

TEST_CASE("SpecReference — T5000 lookup", "[spec_reference]") {
    auto spec = get_spec_reference("T5000");
    REQUIRE(spec.has_value());
    
    CHECK(spec->model == "T5000");
    CHECK(spec->source.find("DS-11945-001") != std::string::npos);
    
    // GPU
    CHECK(spec->gpu_sm_count == 20);
    CHECK(spec->gpu_cuda_cores == 2560);
    CHECK(spec->gpu_tensor_cores == 96);
    CHECK(spec->gpu_boost_clock_ghz == Approx(1.575));
    CHECK(spec->gpu_tmus == 96);
    CHECK(spec->gpu_rops == 32);
    
    // CPU
    CHECK(spec->cpu_core_count == 14);
    CHECK(spec->cpu_max_freq_ghz == Approx(2.6));
    CHECK(spec->cpu_l2_per_core_kb == 1024);
    CHECK(spec->cpu_l3_total_kb == 16 * 1024);
    
    // Memory
    CHECK(spec->memory_total_bytes == 128ULL * 1024 * 1024 * 1024);
    CHECK(spec->memory_type == "LPDDR5X");
    CHECK(spec->memory_peak_bw_gb_s == Approx(273.0));
    CHECK(spec->memory_bus_width_bits == 256);
    
    // Multimedia
    CHECK(spec->nvenc_instance_count == 2);
    CHECK(spec->nvdec_instance_count == 2);
    CHECK(spec->pva_clock_ghz == Approx(1.215));
    
    // I/O
    CHECK(spec->pcie_version == 5);
    CHECK(spec->pcie_max_lanes == 8);
}

TEST_CASE("SpecReference — T4000 lookup", "[spec_reference]") {
    auto spec = get_spec_reference("T4000");
    REQUIRE(spec.has_value());
    
    CHECK(spec->model == "T4000");
    CHECK(spec->gpu_sm_count == 12);
    CHECK(spec->gpu_cuda_cores == 1536);
    CHECK(spec->gpu_tensor_cores == 64);
    CHECK(spec->gpu_boost_clock_ghz == Approx(1.53));
    CHECK(spec->cpu_core_count == 12);
    CHECK(spec->memory_total_bytes == 64ULL * 1024 * 1024 * 1024);
    CHECK(spec->nvenc_instance_count == 1);
    CHECK(spec->nvdec_instance_count == 1);
    CHECK(spec->pva_clock_ghz == Approx(0.0));  // TBD
}

TEST_CASE("SpecReference — case insensitive", "[spec_reference]") {
    auto spec_upper = get_spec_reference("T5000");
    auto spec_lower = get_spec_reference("t5000");
    REQUIRE(spec_upper.has_value());
    REQUIRE(spec_lower.has_value());
    CHECK(spec_upper->gpu_sm_count == spec_lower->gpu_sm_count);
}

TEST_CASE("SpecReference — unknown model returns nullopt", "[spec_reference]") {
    CHECK(!get_spec_reference("Unknown").has_value());
    CHECK(!get_spec_reference("").has_value());
    CHECK(!get_spec_reference("T3000").has_value());
}

TEST_CASE("SpecReference — convenience functions", "[spec_reference]") {
    auto t5 = get_spec_t5000();
    auto t4 = get_spec_t4000();
    
    REQUIRE(t5.has_value());
    REQUIRE(t4.has_value());
    
    CHECK(t5->model == "T5000");
    CHECK(t4->model == "T4000");
    CHECK(t5->gpu_sm_count != t4->gpu_sm_count);
}

TEST_CASE("SpecReference — T5000 vs T4000 differences", "[spec_reference]") {
    auto t5 = get_spec_t5000().value();
    auto t4 = get_spec_t4000().value();
    
    // T5000 has more of everything
    CHECK(t5.gpu_sm_count > t4.gpu_sm_count);
    CHECK(t5.gpu_cuda_cores > t4.gpu_cuda_cores);
    CHECK(t5.gpu_tensor_cores > t4.gpu_tensor_cores);
    CHECK(t5.cpu_core_count > t4.cpu_core_count);
    CHECK(t5.memory_total_bytes > t4.memory_total_bytes);
    CHECK(t5.nvenc_instance_count > t4.nvenc_instance_count);
    
    // Shared specs
    CHECK(t5.cpu_max_freq_ghz == Approx(t4.cpu_max_freq_ghz));
    CHECK(t5.memory_peak_bw_gb_s == Approx(t4.memory_peak_bw_gb_s));
}
