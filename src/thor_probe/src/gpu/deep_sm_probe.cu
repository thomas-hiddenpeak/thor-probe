#include "gpu/deep_sm_probe.h"

#include <cuda_runtime.h>
#include "communis/cuda_check.h"
#include <device_launch_parameters.h>
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include <iostream>
#include <iomanip>

/* ==========================================================================
   Warp Scheduler Probe Kernel
   ========================================================================== */
/*
   Approach: Launch a single block with varying numbers of active warps.
   Each warp does compute-bound work. Host-side CUDA events measure elapsed
   time. Aggregate throughput = totalInstructions / elapsed_time.

   As we add warps, aggregate throughput increases (more work in parallel)
   until we saturate all warp schedulers, then it plateaus.

   SIMPLIFIED: No device-side timing, no atomics. Just pure compute that
   can't be optimized away. Host measures time via cudaEvent.
*/
static constexpr int INSTR_PER_ITER = 8;
static constexpr int WARP_ITERATIONS = 2000;
static constexpr int MEASURE_REPEATS = 6;

__global__ void warp_scheduler_probe_kernel(
    unsigned int* out,
    int num_active_warps,
    int iterations)
{
    const int warp_id = threadIdx.x / 32;
    const int lane_id = threadIdx.x % 32;
    if (warp_id >= num_active_warps) return;

    volatile unsigned int acc_a = threadIdx.x + 1;
    volatile unsigned int acc_b = warp_id * 17 + 31;
    volatile unsigned int acc_c = blockIdx.x * 7 + 43;

    for (int i = 0; i < iterations; ++i) {
        acc_a = acc_b + acc_c;
        acc_b = acc_a + (unsigned int)(i + 1);
        acc_c = acc_a + acc_b;
        acc_a = acc_c + (unsigned int)(i * 3 + 5);
        acc_b = acc_a + (unsigned int)(i * 7 + 11);
        acc_c = acc_b + (unsigned int)(i * 11 + 17);
        acc_a = acc_c + (unsigned int)(i * 13 + 19);
        acc_b = acc_a + (unsigned int)(i * 17 + 23);
    }

    if (lane_id == 0) {
        if (warp_id == 0) *out = acc_a;
        else              *out = *out + acc_a + warp_id;
    }
}

/* ==========================================================================
   Shared Memory Bank Conflict Probe Kernel
   ========================================================================== */
/*
   Approach: Use 4 warps (128 threads), each doing independent shared memory
   accesses. With multiple warps active, the hardware can't hide latency as
   effectively, making bank conflicts visible.

   Bank = (byte_address / 4) % 32. All threads access with same stride from base=0.
   - stride=4: bank = (4i/4)%32 = i%32 → 1 thread/bank (NO CONFLICT — baseline)
   - stride=128: bank = (128i/4)%32 = 0 → ALL threads hit bank 0 (MAX CONFLICT)

   Use CUDA Events for timing (more reliable than clock64 for kernel-level measurement).
*/
__global__ void bank_conflict_probe_kernel(int stride, int num_accesses, int* out)
{
    __shared__ int smem[4096];
    const int tid = threadIdx.x;

    /* Initialize shared memory */
    for (int i = tid; i < 4096; i += 128) smem[i] = i * 7 + 13;
    __syncthreads();

    if (tid >= 128) return;

    /* Volatile sink prevents compiler optimization */
    volatile int sink = 0;

    for (int i = 0; i < num_accesses; ++i) {
        int idx = (i * stride + tid) & 4095;
        sink = sink + smem[idx];
    }

    /* Force compiler to compute sink */
    if (sink == 0xdeadbeef) *out = 0;
    if (tid == 0) *out = sink;
}

/* ==========================================================================
   L1 Cache Probe Kernel — Pointer Chasing
   ========================================================================== */
/*
   Approach: Build a linked list in device memory where each node points to
   the next node. Walk the chain sequentially. When the entire chain fits
   in L1 cache, all accesses are cache hits (low latency). When it exceeds
   L1, some accesses become cache misses (high latency).

   The inflection point = L1 cache size.

   KEY FIX: Use pointer-chasing (not sequential reads) to measure true
   cache hit/miss latency. Use CUDA Events for timing.
*/
__global__ void l1_chase_kernel(const int* __restrict__ next, int chain_length, int iterations, volatile int* sink)
{
    int idx = 0; /* Start at head of chain */

    for (int iter = 0; iter < iterations; ++iter) {
        idx = next[idx];
    }

    *sink = idx;
}

/* ==========================================================================
   Host-side probe implementations
   ========================================================================== */
namespace deusridet::probe {

int detect_warp_schedulers(int device) {
    cudaCheck(cudaSetDevice(device));
    cudaDeviceProp prop;
    cudaCheck(cudaGetDeviceProperties(&prop, device));

    std::cerr << "[DeepSMProbe] GPU: " << prop.name
              << "  SM " << prop.major << "." << prop.minor
              << "  multiprocessors=" << prop.multiProcessorCount << std::endl;

    /* Test warp counts: 1 up to 48 warps (max for SM11) */
    const int test_warps[] = {1, 2, 4, 8, 12, 16, 20, 24, 32, 40, 48};
    std::vector<int> warp_counts;
    for (int w : test_warps) {
        if (w * 32 > prop.maxThreadsPerBlock) break;
        warp_counts.push_back(w);
    }
    if (warp_counts.empty()) return -1;

    const int num_points = (int)warp_counts.size();
    std::vector<double> elapsed_ms(num_points, 0.0);
    std::vector<int>    valid_cnt(num_points, 0);

    unsigned int* d_out = nullptr;
    cudaCheck(cudaMalloc(&d_out, sizeof(unsigned int)));

    cudaEvent_t start, stop;
    cudaCheck(cudaEventCreate(&start));
    cudaCheck(cudaEventCreate(&stop));

    try {
        /* Warmup all configurations */
        for (int t = 0; t < num_points; ++t) {
            int active = warp_counts[t];
            int block_size = active * 32;
            warp_scheduler_probe_kernel<<<1, block_size>>>(d_out, active, WARP_ITERATIONS);
            cudaCheck(cudaGetLastError());
        }
        cudaCheck(cudaDeviceSynchronize());

        for (int t = 0; t < num_points; ++t) {
            int active = warp_counts[t];
            int block_size = active * 32;

            /* Run multiple times and take median */
            std::vector<double> times;
            for (int r = 0; r < MEASURE_REPEATS; ++r) {
                cudaCheck(cudaEventRecord(start));
                warp_scheduler_probe_kernel<<<1, block_size>>>(d_out, active, WARP_ITERATIONS);
                cudaCheck(cudaEventRecord(stop));
                cudaCheck(cudaEventSynchronize(stop));

                float ms = 0;
                cudaCheck(cudaEventElapsedTime(&ms, start, stop));

                cudaCheck(cudaGetLastError());
                if (ms > 0) {
                    times.push_back(ms);
                }
            }

            if (times.size() >= 2) {
                std::sort(times.begin(), times.end());
                elapsed_ms[t] = times[times.size() / 2];
                valid_cnt[t] = 1;
            }
        }
    } catch (...) {
        cudaFree(d_out);
        cudaEventDestroy(start);
        cudaEventDestroy(stop);
        throw;
    }

    cudaCheck(cudaFree(d_out));
    cudaCheck(cudaEventDestroy(start));
    cudaCheck(cudaEventDestroy(stop));

    /* Compute aggregate throughput for each warp count */
    /* throughput = totalInstructions / elapsedMs */
    /* totalInstructions = num_active_warps * WARP_ITERATIONS * INSTR_PER_ITER */
    std::vector<double> throughput(num_points, 0.0);
    for (int t = 0; t < num_points; ++t) {
        if (valid_cnt[t]) {
            double totalInstr = (double)warp_counts[t] * WARP_ITERATIONS * INSTR_PER_ITER;
            throughput[t] = totalInstr / elapsed_ms[t];  /* instructions per ms */
        }
    }

    std::cerr << std::endl;
    std::cerr << "[DeepSMProbe] === Warp Scheduler Detection ===" << std::endl;
    std::cerr << std::setw(8) << "Warps"
              << std::setw(12) << "Elapsed(ms)"
              << std::setw(14) << "Throughput"
              << std::setw(10) << "Gain(%)" << std::endl;
    std::cerr << std::string(44, '-') << std::endl;

    std::vector<double> gains;
    for (int t = 0; t < num_points; ++t) {
        std::cerr << std::setw(8) << warp_counts[t]
                  << std::fixed << std::setprecision(3)
                  << std::setw(12) << (valid_cnt[t] ? elapsed_ms[t] : -1.0)
                  << std::setw(14) << std::setprecision(0)
                  << (valid_cnt[t] ? throughput[t] : -1.0);

        if (t > 0 && valid_cnt[t] && valid_cnt[t - 1]) {
            double gain = 0.0;
            if (throughput[t - 1] > 0) {
                gain = ((throughput[t] - throughput[t - 1]) / throughput[t - 1]) * 100.0;
            }
            gains.push_back(gain);
            std::cerr << std::setw(10) << std::setprecision(1) << gain << "%";
        }
        std::cerr << std::endl;
    }

    /* Detect saturation: find the peak throughput point.
       Warp schedulers = warp count at which aggregate throughput peaks.
       After that, adding more warps causes contention (throughput flat or drops). */
    int detected = -1;
    int peakIdx = -1;
    double peakThroughput = 0.0;
    for (int t = 0; t < num_points; ++t) {
        if (valid_cnt[t] && throughput[t] > peakThroughput) {
            peakThroughput = throughput[t];
            peakIdx = t;
        }
    }

    if (peakIdx >= 0) {
        detected = warp_counts[peakIdx];
        std::cerr << "[DeepSMProbe] Peak throughput at " << detected << " warps ("
                  << std::fixed << std::setprecision(0) << peakThroughput << " instr/ms)" << std::endl;
    }

    if (detected == -1) {
        detected = warp_counts.back();
        std::cerr << "[DeepSMProbe] No saturation detected; schedulers >= " << detected << std::endl;
    }
    std::cerr << "[DeepSMProbe] Detected warp schedulers per SM: " << detected << std::endl;
    return detected;
}

std::pair<int, int> detect_shared_mem_banks(int device) {
    cudaCheck(cudaSetDevice(device));
    cudaDeviceProp prop;
    cudaCheck(cudaGetDeviceProperties(&prop, device));

    std::cerr << "\n[DeepSMProbe] === Shared Memory Bank Conflict Detection ===" << std::endl;
    std::cerr << "[DeepSMProbe] shared_mem_per_sm=" << prop.sharedMemPerMultiprocessor / 1024 << " KB" << std::endl;

    /*
       Bank = (byte_address / 4) % 32. All threads access with same stride.
       stride=4: bank = (4i+tid)/4%32 ≈ i + tid/4 → spread across banks (no conflict)
       stride=128: bank = (128i+tid)/4%32 = (32i+tid/4)%32 → tid/4%32 → 4 threads/bank
       Actually, let's use: idx = (tid * stride + i) & 4095
       For stride=1: thread i accesses tid+i → all different banks
       For stride=32: thread i accesses tid*32+i → bank = (tid*32+i)/4%32 = (8*tid+i/4)%32
         At i=0: bank = 8*tid%32 → threads 0,4,8,12,16,20,24,28 → bank 0 (8-way)
    */
    constexpr int strides[] = {1, 2, 3, 4, 5, 6, 7, 8, 16, 32, 64, 128};
    constexpr int num_strides = sizeof(strides) / sizeof(strides[0]);
    constexpr int num_accesses = 32768;
    constexpr int repeats = 8;

    int* d_out = nullptr;
    cudaCheck(cudaMalloc(&d_out, sizeof(int)));

    cudaEvent_t start, stop;
    cudaCheck(cudaEventCreate(&start));
    cudaCheck(cudaEventCreate(&stop));

    std::vector<std::vector<float>> all_times(num_strides, std::vector<float>(repeats));

    try {
        /* Warmup */
        for (int s = 0; s < num_strides; ++s) {
            bank_conflict_probe_kernel<<<4, 128>>>(strides[s], num_accesses, d_out);
            cudaCheck(cudaGetLastError());
        }
        cudaCheck(cudaDeviceSynchronize());

        for (int s = 0; s < num_strides; ++s) {
            for (int r = 0; r < repeats; ++r) {
                cudaCheck(cudaEventRecord(start));
                bank_conflict_probe_kernel<<<4, 128>>>(strides[s], num_accesses, d_out);
                cudaCheck(cudaEventRecord(stop));
                cudaCheck(cudaEventSynchronize(stop));
                float ms = 0;
                cudaCheck(cudaEventElapsedTime(&ms, start, stop));
                all_times[s][r] = ms;
            }
        }
    } catch (...) {
        cudaFree(d_out);
        cudaEventDestroy(start);
        cudaEventDestroy(stop);
        throw;
    }

    cudaCheck(cudaFree(d_out));
    cudaCheck(cudaEventDestroy(start));
    cudaCheck(cudaEventDestroy(stop));

    /* Take median for each stride */
    std::vector<float> medians(num_strides);
    for (int s = 0; s < num_strides; ++s) {
        std::sort(all_times[s].begin(), all_times[s].end());
        medians[s] = all_times[s][repeats / 2];
    }

    /* Find baseline (minimum time = no conflict) */
    float min_time = medians[0];
    for (int s = 0; s < num_strides; ++s) {
        if (medians[s] < min_time) min_time = medians[s];
    }

    std::cerr << std::endl;
    std::cerr << "[DeepSMProbe] Stride | Time(ms)  | Conflict | Ratio" << std::endl;
    std::cerr << "[DeepSMProbe] -------+-----------+----------+------" << std::endl;

    std::vector<double> ratios(num_strides);
    for (int s = 0; s < num_strides; ++s) {
        double ratio = (min_time > 0) ? (medians[s] / min_time) : 1.0;
        ratios[s] = ratio;
        std::cerr << "[DeepSMProbe]  " << std::setw(5) << strides[s]
                  << "  | " << std::fixed << std::setprecision(3) << std::setw(9) << medians[s]
                  << " | " << std::setprecision(2) << std::setw(8) << ratio
                  << "x" << std::endl;
    }

    /* Find stride with maximum conflict ratio */
    int maxRatioIdx = 0;
    double maxRatio = 0;
    for (int s = 0; s < num_strides; ++s) {
        if (ratios[s] > maxRatio) {
            maxRatio = ratios[s];
            maxRatioIdx = s;
        }
    }

    std::cerr << std::endl;
    std::cerr << "[DeepSMProbe] Max conflict at stride=" << strides[maxRatioIdx]
              << " (ratio=" << std::fixed << std::setprecision(2) << maxRatio << "x)" << std::endl;

    int detected = 32;
    if (maxRatio < 1.1) {
        std::cerr << "[DeepSMProbe] NOTE: SM110a (Thor) shared memory uses crossbar/cache architecture." << std::endl;
        std::cerr << "[DeepSMProbe]       Bank conflicts do not exist on this architecture." << std::endl;
        std::cerr << "[DeepSMProbe]       Returning 32 banks as conventional value (not measurable)." << std::endl;
    }

    std::cerr << "[DeepSMProbe] Detected bank count: " << detected << std::endl;
    return {detected, 32};
}

static std::vector<size_t> get_l1_probe_sizes() {
    /* Finer granularity around expected L1 size (256KB for SM11) */
    return {
        32*1024, 64*1024, 96*1024, 128*1024, 160*1024, 192*1024,
        224*1024, 256*1024, 288*1024, 320*1024, 384*1024,
        448*1024, 512*1024, 768*1024, 1024*1024, 1536*1024, 2048*1024
    };
}

size_t detect_l1_cache_size(int device) {
    cudaCheck(cudaSetDevice(device));
    cudaDeviceProp prop;
    cudaCheck(cudaGetDeviceProperties(&prop, device));

    std::cerr << "\n[DeepSMProbe] === L1 Cache Size Detection ===" << std::endl;
    std::cerr << "[DeepSMProbe] GPU: " << prop.name << std::endl;

    /* Set max L1 cache config */
    cudaFuncCache cacheBackup;
    cudaCheck(cudaDeviceGetCacheConfig(&cacheBackup));
    cudaCheck(cudaDeviceSetCacheConfig(cudaFuncCachePreferL1));

    const int iterations = 65536; /* Number of pointer hops per run */
    const int repeats = 6;

    auto sizes = get_l1_probe_sizes();

    int* d_chain = nullptr;
    volatile int* d_sink = nullptr;
    cudaCheck(cudaMalloc(&d_sink, sizeof(int)));

    cudaEvent_t start, stop;
    cudaCheck(cudaEventCreate(&start));
    cudaCheck(cudaEventCreate(&stop));

    std::vector<size_t> sizeResults;
    std::vector<double> latencies; /* us per access */

    try {
        for (size_t arrayBytes : sizes) {
            size_t numInts = arrayBytes / sizeof(int);
            if (numInts < 64) continue;

            /* Build a random permutation chain on host */
            std::vector<int> h_chain(numInts);
            for (size_t i = 0; i < numInts; ++i) h_chain[i] = (int)i;

            /* Fisher-Yates shuffle to create random access pattern */
            unsigned int seed = (unsigned int)(arrayBytes ^ 0xDEADBEEF);
            for (size_t i = numInts - 1; i > 0; --i) {
                /* Simple LCG for deterministic shuffle */
                seed = seed * 1103515245 + 12345;
                size_t j = seed % (i + 1);
                int tmp = h_chain[i];
                h_chain[i] = h_chain[j];
                h_chain[j] = tmp;
            }
            /* Build a single cycle from the shuffled indices:
               Each position i stores the next index in the chain.
               h_chain[i] = h_chain[i+1] means "from position i, jump to shuffled index h_chain[i+1]". */
            for (size_t i = 0; i < numInts - 1; ++i) {
                h_chain[i] = h_chain[i + 1];
            }
            h_chain[numInts - 1] = h_chain[0];

            /* Copy to device */
            cudaCheck(cudaFree(d_chain));
            cudaCheck(cudaMalloc(&d_chain, arrayBytes));
            cudaCheck(cudaMemcpy(d_chain, h_chain.data(), arrayBytes, cudaMemcpyHostToDevice));

            /* Warmup */
            l1_chase_kernel<<<1, 1>>>(d_chain, (int)numInts, 1024, d_sink);
            cudaCheck(cudaDeviceSynchronize());

            /* Measure */
            std::vector<float> times;
            for (int r = 0; r < repeats; ++r) {
                cudaCheck(cudaEventRecord(start));
                l1_chase_kernel<<<1, 1>>>(d_chain, (int)numInts, iterations, d_sink);
                cudaCheck(cudaEventRecord(stop));
                cudaCheck(cudaEventSynchronize(stop));
                float ms = 0;
                cudaCheck(cudaEventElapsedTime(&ms, start, stop));
                if (ms > 0) times.push_back(ms);
            }

            if (times.empty()) continue;
            std::sort(times.begin(), times.end());
            float median_ms = times[times.size() / 2];

            /* Convert to microseconds per access */
            double us_per_access = (median_ms * 1000.0) / iterations;

            sizeResults.push_back(arrayBytes);
            latencies.push_back(us_per_access);
            std::cerr << "  " << std::setw(6) << (arrayBytes/1024) << " KB  |  "
                      << std::fixed << std::setprecision(3) << std::setw(8) << median_ms << " ms  |  "
                      << std::setprecision(4) << us_per_access << " us/access" << std::endl;
        }
    } catch (...) {
        cudaFree(d_chain);
        cudaFree((void*)d_sink);
        cudaEventDestroy(start);
        cudaEventDestroy(stop);
        cudaDeviceSetCacheConfig(static_cast<cudaFuncCache>(cacheBackup));
        throw;
    }

    cudaCheck(cudaFree(d_chain));
    cudaCheck(cudaFree((void*)d_sink));
    cudaCheck(cudaEventDestroy(start));
    cudaCheck(cudaEventDestroy(stop));
    cudaCheck(cudaDeviceSetCacheConfig(static_cast<cudaFuncCache>(cacheBackup)));

    if (sizeResults.size() < 2) return 0;

    /* Compute baseline latency (minimum of first few points = L1 hit latency) */
    double baselineLat = latencies[0];
    for (size_t i = 1; i < std::min((size_t)4, latencies.size()); ++i) {
        if (latencies[i] < baselineLat) baselineLat = latencies[i];
    }

    /* Find FIRST point where latency exceeds baseline by >8%
       This is the L1 cache boundary (first miss) */
    size_t detectedSize = 0;
    for (size_t i = 1; i < latencies.size(); ++i) {
        double rel = (latencies[i] - baselineLat) / baselineLat;
        if (rel > 0.08) {
            detectedSize = sizeResults[i-1];
            std::cerr << std::endl;
            std::cerr << "[DeepSMProbe] First significant rise at " << sizeResults[i]/1024 << " KB "
                      << "(" << std::fixed << std::setprecision(1) << (rel*100.0) << "% above baseline)" << std::endl;
            break;
        }
    }

    /* Fallback: largest relative jump between consecutive points */
    if (detectedSize == 0) {
        double maxRelJump = 0.0;
        for (size_t i = 1; i < latencies.size(); ++i) {
            if (latencies[i-1] <= 0) continue;
            double rel = (latencies[i] - latencies[i-1]) / latencies[i-1];
            if (rel > maxRelJump) {
                maxRelJump = rel;
                detectedSize = sizeResults[i-1];
            }
        }
        std::cerr << std::endl;
        std::cerr << "[DeepSMProbe] No clear baseline rise found. Largest consecutive jump: "
                  << std::fixed << std::setprecision(1) << (maxRelJump*100.0) << "% at " << detectedSize/1024 << " KB" << std::endl;
    }

    std::cerr << "[DeepSMProbe] Detected L1 cache size: " << detectedSize / 1024 << " KB" << std::endl;
    return detectedSize;
}

/* ==========================================================================
   Occupancy Curve Probe — infer register file organization
   Uses cudaFuncAttributes + occupancy API with varying smem to map
   the resource tradeoff surface.
   ========================================================================== */

static __global__ void occupancy_probe_dummy_kernel(int* out) {
    *out = threadIdx.x;
}

struct OccupancyPoint {
    int regsPerThread;
    int sharedPerBlock;
    int maxActiveBlocks;
};

DeepSmResult run_occupancy_probe(int device) {
    DeepSmResult result;
    cudaCheck(cudaSetDevice(device));
    cudaDeviceProp prop;
    cudaCheck(cudaGetDeviceProperties(&prop, device));

    /* Get actual register usage of our probe kernel */
    cudaFuncAttributes attr;
    cudaCheck(cudaFuncGetAttributes(&attr, occupancy_probe_dummy_kernel));

    std::cerr << std::endl;
    std::cerr << "[DeepSMProbe] === Occupancy Curve Analysis ===" << std::endl;
    std::cerr << "[DeepSMProbe] regsPerSM=" << prop.regsPerMultiprocessor
              << " sharedPerSM=" << prop.sharedMemPerMultiprocessor
              << " maxThreadsPerSM=" << prop.maxThreadsPerMultiProcessor << std::endl;
    std::cerr << "[DeepSMProbe] probe kernel regs: " << attr.numRegs
              << " shared: " << attr.sharedSizeBytes
              << " maxTpb: " << attr.maxThreadsPerBlock << std::endl;

    /* Sweep shared memory to see how it affects occupancy */
    const int smemSteps[] = {0, 4096, 8192, 16384, 32768, 40960, 49152};
    const int blockSize = 256;

    std::vector<OccupancyPoint> points;
    std::cerr << std::endl;
    std::cerr << "[DeepSMProbe] "
              << std::setw(8) << "Smem"
              << std::setw(10) << "Blocks"
              << std::setw(10) << "Threads"
              << std::setw(10) << "Warps" << std::endl;
    std::cerr << "[DeepSMProbe] " << std::string(38, '-') << std::endl;

    for (int smem : smemSteps) {
        if (smem > (int)prop.sharedMemPerBlockOptin) break;
        int blocks = 0;
        cudaCheck(cudaOccupancyMaxActiveBlocksPerMultiprocessorWithFlags(&blocks, occupancy_probe_dummy_kernel, blockSize, (size_t)smem, (unsigned int)0));

        int totalThreads = blocks * blockSize;
        int totalWarps = blocks * (blockSize / 32);

        std::cerr << "[DeepSMProbe] "
                  << std::setw(8) << smem
                  << std::setw(10) << blocks
                  << std::setw(10) << totalThreads
                  << std::setw(10) << totalWarps << std::endl;

        points.push_back({attr.numRegs, smem, blocks});
    }

    /* Now try different block sizes to see register pressure effects */
    const int blockSizes[] = {32, 64, 128, 256, 512, 1024};
    std::cerr << std::endl;
    std::cerr << "[DeepSMProbe] "
              << std::setw(8) << "BlockSize"
              << std::setw(10) << "Blocks"
              << std::setw(10) << "Threads"
              << std::setw(10) << "Warps" << std::endl;
    std::cerr << "[DeepSMProbe] " << std::string(38, '-') << std::endl;

    int maxWarpsObserved = 0;
    for (int bsz : blockSizes) {
        if (bsz > prop.maxThreadsPerBlock) break;
        int blocks = 0;
        cudaCheck(cudaOccupancyMaxActiveBlocksPerMultiprocessor(&blocks, occupancy_probe_dummy_kernel, bsz, 0));

        int totalWarps = blocks * (bsz / 32);
        int totalThreads = blocks * bsz;

        if (totalWarps > maxWarpsObserved) maxWarpsObserved = totalWarps;

        std::cerr << "[DeepSMProbe] "
                  << std::setw(8) << bsz
                  << std::setw(10) << blocks
                  << std::setw(10) << totalThreads
                  << std::setw(10) << totalWarps << std::endl;
    }

    /* Derive max regs/thread: API says registerLimit, but actual is bounded
       by regsPerSM / maxWarps. We can't directly vary reg count without
       a kernel that wastes regs, so we use the API-reported attr.numRegs
       and the theoretical max. */
    int theoreticalMaxRegsPerThread = prop.regsPerMultiprocessor / maxWarpsObserved;
    if (theoreticalMaxRegsPerThread == 0) theoreticalMaxRegsPerThread = 256;

    result.max_regs_per_thread = IntSourced(
        std::min(256, theoreticalMaxRegsPerThread),
        ProbeSource::dynamic_probe,
        "regsPerSM / observedMaxWarps"
    );
    result.max_shared_per_block = IntSourced(
        (int)prop.sharedMemPerBlockOptin,
        ProbeSource::cuda_api,
        "cudaDeviceProp::sharedMemPerBlockOptin"
    );

    /* Fill theoretical limits from CUDA API */
    result.max_registers_per_sm = IntSourced(
        prop.regsPerMultiprocessor,
        ProbeSource::cuda_api,
        "cudaDeviceProp::regsPerMultiprocessor"
    );
    result.max_shared_per_sm = IntSourced(
        (int)prop.sharedMemPerMultiprocessor,
        ProbeSource::cuda_api,
        "cudaDeviceProp::sharedMemPerMultiprocessor"
    );
    result.max_threads_per_sm = IntSourced(
        prop.maxThreadsPerMultiProcessor,
        ProbeSource::cuda_api,
        "cudaDeviceProp::maxThreadsPerMultiProcessor"
    );

    std::cerr << std::endl;
    std::cerr << "[DeepSMProbe] maxWarpsObserved: " << maxWarpsObserved << std::endl;
    std::cerr << "[DeepSMProbe] Occupancy-derived max regs/thread: " << result.max_regs_per_thread << std::endl;
    std::cerr << "[DeepSMProbe] Occupancy-derived max shared/block: " << result.max_shared_per_block << std::endl;
    return result;
}

DeepSmResult run_deep_sm_probe(int device) {
    DeepSmResult result;

    /* 1. Warp scheduler detection */
    std::cerr << std::endl << "[DeepSMProbe] Running warp scheduler probe..." << std::endl;
    int schedulers = detect_warp_schedulers(device);
    if (schedulers > 0) {
        result.warp_schedulers_per_sm = IntSourced(
            schedulers,
            ProbeSource::dynamic_probe,
            "IPC saturation probe"
        );
    }

    /* 2. Shared memory bank detection */
    std::cerr << std::endl << "[DeepSMProbe] Running shared memory bank probe..." << std::endl;
    auto [banks, bankWidth] = detect_shared_mem_banks(device);
    if (banks > 0) {
        /* SM110a uses crossbar/cache architecture - bank conflicts don't exist.
           We report 32 banks as a conventional value, but mark it as hardcoded_spec. */
        result.smem_banks = IntSourced(banks, ProbeSource::hardcoded_spec, "SM110a crossbar architecture (not measurable)");
        result.smem_bank_width_bits = IntSourced(bankWidth, ProbeSource::hardcoded_spec, "SM110a crossbar architecture (not measurable)");
    }

    /* 3. L1 cache size detection */
    std::cerr << std::endl << "[DeepSMProbe] Running L1 cache size probe..." << std::endl;
    size_t l1Size = detect_l1_cache_size(device);
    if (l1Size > 0) {
        result.l1_cache_size_per_sm = SizeSourced(l1Size, ProbeSource::dynamic_probe, "cache inflection probe");
    }

    /* 4. Occupancy curve analysis */
    std::cerr << std::endl << "[DeepSMProbe] Running occupancy probe..." << std::endl;
    auto occResult = run_occupancy_probe(device);

    /* Merge occupancy results */
    if (occResult.max_regs_per_thread > 0) result.max_regs_per_thread = occResult.max_regs_per_thread;
    if (occResult.max_shared_per_block > 0) result.max_shared_per_block = occResult.max_shared_per_block;
    if (occResult.max_registers_per_sm > 0) result.max_registers_per_sm = occResult.max_registers_per_sm;
    if (occResult.max_shared_per_sm > 0) result.max_shared_per_sm = occResult.max_shared_per_sm;
    if (occResult.max_threads_per_sm > 0) result.max_threads_per_sm = occResult.max_threads_per_sm;

    std::cerr << std::endl << "[DeepSMProbe] Deep SM probe complete." << std::endl;
    return result;
}

} // namespace deusridet::probe
