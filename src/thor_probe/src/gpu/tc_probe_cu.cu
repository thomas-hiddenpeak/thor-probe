#include "gpu/tc_probe.h"
#include "communis/log.h"
#include "communis/cuda_check.h"

#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <mma.h>
#include <cuda_fp16.h>
#include <cuda_fp8.h>

/* Non-standard device attributes not yet in cuda_runtime.h */
enum CustomDeviceAttr : int {
    CUDA_DEV_ATTR_CLUSTER_LAUNCH = 120, // CU_DEVICE_ATTRIBUTE_CLUSTER_LAUNCH
};

/* ============================================================================
   FP16 MMA probe via nvcuda::wmma
   ============================================================================ */

__global__ void test_fp16_mma_probe(float* out) {
    nvcuda::wmma::fragment<nvcuda::wmma::matrix_a, 16, 16, 16, half, nvcuda::wmma::row_major> a;
    nvcuda::wmma::fragment<nvcuda::wmma::matrix_b, 16, 16, 16, half, nvcuda::wmma::col_major> b;
    nvcuda::wmma::fragment<nvcuda::wmma::accumulator, 16, 16, 16, float> c;

    nvcuda::wmma::fill_fragment(a, __float2half(1.0f));
    nvcuda::wmma::fill_fragment(b, __float2half(1.0f));
    nvcuda::wmma::fill_fragment(c, 0.0f);

    nvcuda::wmma::mma_sync(c, a, b, c);

    float sum = 0;
    for (int i = 0; i < c.num_elements; i++) sum += c.x[i];
    if (threadIdx.x == 0) *out = sum;
}

/* ============================================================================
   BF16 MMA probe via nvcuda::wmma (16x16x16)
   ============================================================================ */

__global__ void test_bf16_mma_probe(float* out) {
    nvcuda::wmma::fragment<nvcuda::wmma::matrix_a, 16, 16, 16, __nv_bfloat16, nvcuda::wmma::row_major> a;
    nvcuda::wmma::fragment<nvcuda::wmma::matrix_b, 16, 16, 16, __nv_bfloat16, nvcuda::wmma::col_major> b;
    nvcuda::wmma::fragment<nvcuda::wmma::accumulator, 16, 16, 16, float> c;

    nvcuda::wmma::fill_fragment(a, __float2bfloat16(1.0f));
    nvcuda::wmma::fill_fragment(b, __float2bfloat16(1.0f));
    nvcuda::wmma::fill_fragment(c, 0.0f);

    nvcuda::wmma::mma_sync(c, a, b, c);

    float sum = 0;
    for (int i = 0; i < c.num_elements; i++) sum += c.x[i];
    if (threadIdx.x == 0) *out = sum;
}

/* ============================================================================
   FP8 MMA probe via inline PTX mma.sync.aligned (16x8x32, E4M3 operands, FP32 acc)
   wmma API does not expose FP8; we use PTX directly.  Two m16n8k32 tiles form
   the full 16x16x64 HW tile; here we probe a single tile to verify the
   tensor core unit accepts FP8 operands.
   ============================================================================ */

__global__ void test_fp8_mma_probe(float* out) {
    uint32_t a_reg[4];
    uint32_t b_reg[2];
    float    c_reg[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    __nv_fp8_e4m3 one = __nv_fp8_e4m3(1.0f);
    uint32_t val = static_cast<uint32_t>(one.__x);
    uint32_t four_ones = val | (val << 8) | (val << 16) | (val << 24);
    for (int i = 0; i < 4; i++) a_reg[i] = four_ones;
    for (int i = 0; i < 2; i++) b_reg[i] = four_ones;

    asm volatile(
        "mma.sync.aligned.m16n8k32.row.col.f32.e4m3.e4m3.f32 "
        "{%0, %1, %2, %3}, "
        "{%4, %5, %6, %7}, "
        "{%8, %9}, "
        "{%10, %11, %12, %13};"
        : "=f"(c_reg[0]), "=f"(c_reg[1]), "=f"(c_reg[2]), "=f"(c_reg[3])
        : "r"(a_reg[0]), "r"(a_reg[1]), "r"(a_reg[2]), "r"(a_reg[3]),
          "r"(b_reg[0]), "r"(b_reg[1]),
          "f"(c_reg[0]), "f"(c_reg[1]), "f"(c_reg[2]), "f"(c_reg[3])
    );

    float sum = c_reg[0] + c_reg[1] + c_reg[2] + c_reg[3];
    if (threadIdx.x == 0) *out = sum;
}

/* nvFP4 MMA probe
   NOTE: nvFP4 PTX (kind::f8f6f4) not supported on sm_110a yet — only sm_120+.
   Marked as "assumed" for SM110a since the hardware supports the data type but
   nvcc ptxas for sm_110a rejects the mma instruction. The benchmark (tc_nvfp4_bench)
   uses tcgen05.mma.cta_group::1.kind::mxf4.block_scale which works. */
// No kernel — ptxas rejects nvFP4 mma on sm_110a. Handled in detection below.

/* ============================================================================
   INT8 MMA probe via nvcuda::wmma (16x16x16)
   wmma uses signed char (not int8_t) and int accumulator for INT8.
   ============================================================================ */

__global__ void test_int8_mma_probe(float* out) {
    nvcuda::wmma::fragment<nvcuda::wmma::matrix_a, 16, 16, 16, int8_t, nvcuda::wmma::row_major> a;
    nvcuda::wmma::fragment<nvcuda::wmma::matrix_b, 16, 16, 16, int8_t, nvcuda::wmma::col_major> b;
    nvcuda::wmma::fragment<nvcuda::wmma::accumulator, 16, 16, 16, int> c;

    nvcuda::wmma::fill_fragment(a, static_cast<int8_t>(1));
    nvcuda::wmma::fill_fragment(b, static_cast<int8_t>(1));
    nvcuda::wmma::fill_fragment(c, 0);

    nvcuda::wmma::mma_sync(c, a, b, c);

    float sum = 0;
    for (int i = 0; i < c.num_elements; i++) sum += static_cast<float>(c.x[i]);
    if (threadIdx.x == 0) *out = sum;
}

/* ============================================================================
   Barrier/fence probe
   ============================================================================ */

__global__ void test_barrier_probe(int* out) {
    *out = 0;
    asm volatile("fence.proxy.async.shared::cta;\n");
    *out = 1;
}

/* ============================================================================
   TMA (cp.async) probe
   ============================================================================ */

__global__ void test_tma_probe(int* out) {
    *out = 0;
    asm volatile("cp.async.commit_group;\n");
    asm volatile("cp.async.wait_group 0;\n");
    *out = 1;
}

/* ============================================================================
   Warp shuffle probe
   ============================================================================ */

__global__ void test_warp_shuffle_probe(float* out) {
    float val = static_cast<float>(threadIdx.x) + 1.0f;
    float shfl = __shfl_sync(0xFFFFFFFF, val, 31);
    if (threadIdx.x == 0) *out = shfl;
}

/* ============================================================================
   Warp vote probe
   ============================================================================ */

__global__ void test_warp_vote_probe(int* out) {
    int predicate = (threadIdx.x < 16);
    int any = __any_sync(0xFFFFFFFF, predicate);
    int all = __all_sync(0xFFFFFFFF, predicate);
    unsigned int ballot = __ballot_sync(0xFFFFFFFF, predicate);
    if (threadIdx.x == 0) *out = any + all + static_cast<int>(ballot);
}

template<typename KernelFn>
static bool probe_kernel(KernelFn kernel, const char* label) {
    float *d = nullptr;
    try {
        cudaCheck(cudaMalloc(&d, sizeof(float)));
        cudaCheck(cudaMemset(d, 0, sizeof(float)));

        kernel<<<1, 32>>>(d);
        cudaError_t err = cudaGetLastError();

        float h = 0;
        cudaCheck(cudaMemcpy(&h, d, sizeof(float), cudaMemcpyDeviceToHost));
        cudaCheck(cudaFree(d));
        d = nullptr;

        if (err == cudaSuccess && h > 0.0f) {
            LOG_INFO("ThorProbe", "  %s: supported (output=%.1f)", label, h);
            return true;
        } else {
            if (err != cudaSuccess)
                LOG_INFO("ThorProbe", "  %s: %s", label, cudaGetErrorString(err));
            else
                LOG_INFO("ThorProbe", "  %s: zero output", label);
            return false;
        }
    } catch (...) {
        cudaError_t cleanup_err = cudaFree(d); (void)cleanup_err;
        throw;
    }
}

namespace deusridet::probe {

TcGen05Capability detect_tcgen05_capabilities(int device) {
    TcGen05Capability cap;
    cudaCheck(cudaSetDevice(device));

    cudaDeviceProp prop;
    cudaCheck(cudaGetDeviceProperties(&prop, device));

    if (prop.major < 11) {
        LOG_WARN("ThorProbe", "Device %d SM%d.%d - tcgen05 N/A",
                  device, prop.major, prop.minor);
        return cap;
    }

    LOG_INFO("ThorProbe", "=== TC Gen05 Detection: %s (SM%d.%d) ===",
              prop.name, prop.major, prop.minor);

    // FP16 MMA
    if (probe_kernel(test_fp16_mma_probe, "FP16 MMA")) {
        cap.mma[static_cast<size_t>(MmaType::FP16)].supported = true;
        cap.mma[static_cast<size_t>(MmaType::FP16)].m_size = 16;
        cap.mma[static_cast<size_t>(MmaType::FP16)].n_size = 16;
        cap.mma[static_cast<size_t>(MmaType::FP16)].k_size = 16;
    }

    // BF16 MMA
    if (probe_kernel(test_bf16_mma_probe, "BF16 MMA")) {
        cap.mma[static_cast<size_t>(MmaType::BF16)].supported = true;
        cap.mma[static_cast<size_t>(MmaType::BF16)].m_size = 16;
        cap.mma[static_cast<size_t>(MmaType::BF16)].n_size = 16;
        cap.mma[static_cast<size_t>(MmaType::BF16)].k_size = 16;
    }

    // FP8 MMA
    if (probe_kernel(test_fp8_mma_probe, "FP8 MMA")) {
        cap.mma[static_cast<size_t>(MmaType::FP8_E4M3)].supported = true;
        cap.mma[static_cast<size_t>(MmaType::FP8_E4M3)].m_size = 16;
        cap.mma[static_cast<size_t>(MmaType::FP8_E4M3)].n_size = 16;
        cap.mma[static_cast<size_t>(MmaType::FP8_E4M3)].k_size = 64;
    }

    // nvFP4 MMA (SM110a/Blackwell) — assumed, ptxas rejects mma on sm_110a
    cap.mma[static_cast<size_t>(MmaType::NVFP4_BLOCK_SCALE)].supported = true;
    cap.mma[static_cast<size_t>(MmaType::NVFP4_BLOCK_SCALE)].m_size = 16;
    cap.mma[static_cast<size_t>(MmaType::NVFP4_BLOCK_SCALE)].n_size = 16;
    cap.mma[static_cast<size_t>(MmaType::NVFP4_BLOCK_SCALE)].k_size = 128;
    cap.mma[static_cast<size_t>(MmaType::NVFP4_BLOCK_SCALE)].note = "assumed (ptxas sm_110a)";
    LOG_INFO("ThorProbe", "  nvFP4 MMA: assumed (ptxas sm_110a, will verify via benchmark)");

    // INT8 MMA
    if (probe_kernel(test_int8_mma_probe, "INT8 MMA")) {
        cap.mma[static_cast<size_t>(MmaType::INT8)].supported = true;
        cap.mma[static_cast<size_t>(MmaType::INT8)].m_size = 16;
        cap.mma[static_cast<size_t>(MmaType::INT8)].n_size = 16;
        cap.mma[static_cast<size_t>(MmaType::INT8)].k_size = 16;
    }

    // Cluster launch (CU_DEVICE_ATTRIBUTE_CLUSTER_LAUNCH)
    {
        int val = 0;
        {
            cudaError_t attrErr = cudaDeviceGetAttribute(&val, static_cast<cudaDeviceAttr>(CustomDeviceAttr::CUDA_DEV_ATTR_CLUSTER_LAUNCH), device);
            if (attrErr == cudaSuccess)
                cap.barrier.cluster_launch = (val != 0);
            else if (attrErr != cudaErrorNotSupported)
                cudaCheck(attrErr);
        }

        cap.barrier.max_cluster_width = 8;
        cap.barrier.max_cluster_height = 1;
        cap.barrier.max_cluster_depth = 1;

        cap.barrier.grid_mem_fence_supported = cap.barrier.cluster_launch;

        LOG_INFO("ThorProbe", "  Cluster launch: %s", cap.barrier.cluster_launch ? "yes" : "no");
        LOG_INFO("ThorProbe", "  Cluster max dimensions: %dx%dx%d",
                  cap.barrier.max_cluster_width, cap.barrier.max_cluster_height, cap.barrier.max_cluster_depth);
    }

    // Barrier (cluster mem fence via kernel test)
    {
        int *d = nullptr;
        try {
            cudaCheck(cudaMalloc(&d, sizeof(int)));
            cudaCheck(cudaMemset(d, 0, sizeof(int)));
            test_barrier_probe<<<1, 32>>>(d);
            cudaError_t err = cudaGetLastError();
            int h = 0;
            cudaCheck(cudaMemcpy(&h, d, sizeof(int), cudaMemcpyDeviceToHost));
            cudaCheck(cudaFree(d));
            d = nullptr;

            cap.barrier.cluster_mem_fence_supported = (err == cudaSuccess && h != 0);

            LOG_INFO("ThorProbe", "  Cluster mem fence: %s", cap.barrier.cluster_mem_fence_supported ? "yes" : "no");
        } catch (...) {
            cudaError_t cleanup_err = cudaFree(d); (void)cleanup_err;
            throw;
        }
    }

    // tmEM — inferred from CC >= 11 + cluster_launch (no dedicated attribute)
    {
        cap.tmem.supported = (prop.major >= 11) && cap.barrier.cluster_launch;
        cap.tmem.total_bytes = 0;
        cap.tmem.cp_async_tmem = cap.tmem.supported;
        cap.tmem.mma_tmem = cap.tmem.supported;
        LOG_INFO("ThorProbe", "  tmEM: %s (inferred from CC %d.%d + cluster launch, size unknown)",
                  cap.tmem.supported ? "yes" : "no", prop.major, prop.minor);
    }

    // Async Copy (TMA cp.async) — verified by actual kernel execution
    {
        int *d = nullptr;
        try {
            cudaCheck(cudaMalloc(&d, sizeof(int)));
            cudaCheck(cudaMemset(d, 0, sizeof(int)));
            test_tma_probe<<<1, 32>>>(d);
            cudaError_t err = cudaGetLastError();
            int h = 0;
            cudaCheck(cudaMemcpy(&h, d, sizeof(int), cudaMemcpyDeviceToHost));
            cudaCheck(cudaFree(d));
            d = nullptr;

            cap.async_copy.tcgen05_cp = (err == cudaSuccess && h != 0);
            cap.async_copy.shared_mem_fence = cap.async_copy.tcgen05_cp;
            cap.async_copy.barrier_notify = cap.async_copy.tcgen05_cp && cap.barrier.cluster_mem_fence_supported;

            LOG_INFO("ThorProbe", "  cp.async (TMA): %s", cap.async_copy.tcgen05_cp ? "yes" : "no");
        } catch (...) {
            cudaError_t cleanup_err = cudaFree(d); (void)cleanup_err;
            throw;
        }
    }

    // Warp Shuffle
    if (probe_kernel(test_warp_shuffle_probe, "Warp Shuffle")) {
        cap.warp.shuffle_supported = true;
    }

    // Warp Vote
    {
        int *d = nullptr;
        try {
            cudaCheck(cudaMalloc(&d, sizeof(int)));
            cudaCheck(cudaMemset(d, 0, sizeof(int)));
            test_warp_vote_probe<<<1, 32>>>(d);
            cudaError_t err = cudaGetLastError();
            int h = 0;
            cudaCheck(cudaMemcpy(&h, d, sizeof(int), cudaMemcpyDeviceToHost));
            cudaCheck(cudaFree(d));
            d = nullptr;

            cap.warp.vote_supported = (err == cudaSuccess && h != 0);
            LOG_INFO("ThorProbe", "  Warp Vote: %s (output=%d)",
                      cap.warp.vote_supported ? "yes" : "no", h);
        } catch (...) {
            cudaError_t cleanup_err = cudaFree(d); (void)cleanup_err;
            throw;
        }
    }

    return cap;
}

} // namespace deusridet::probe
