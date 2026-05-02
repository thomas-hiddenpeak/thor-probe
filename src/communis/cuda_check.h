#pragma once
#include <cuda_runtime.h>
#include <stdexcept>
#include <string>
#include "log.h"

#define cudaCheck(err)                                                                                                    \
    do {                                                                                                                              \
        cudaError_t status = (err);                                                                                                   \
        if (status != cudaSuccess) {                                                                                                  \
            const char* msg = cudaGetErrorString(status);                                                                             \
            LOG_ERROR("CUDA", "cudaCheck failed at %s:%d: %s",                                                                        \
                      __FILE__, __LINE__, msg);                                                                                       \
            throw std::runtime_error(std::string("CUDA error: ") + msg);                                      \
        }                                                                                                                                       \
    } while (0)

// cudaCheckNoThrow: Non-throwing variant for use in noexcept contexts (e.g. destructors).
// Throws in a noexcept destructor -> std::terminate(). Use this instead when exceptions
// are not an option; it logs the error and returns silently.
#define cudaCheckNoThrow(err)                                                                                             \
    do {                                                                                                                              \
        cudaError_t status = (err);                                                                                                   \
        if (status != cudaSuccess) {                                                                                                  \
            const char* msg = cudaGetErrorString(status);                                                                             \
            LOG_ERROR("CUDA", "cudaCheckNoThrow failed at %s:%d: %s",                                                                 \
                      __FILE__, __LINE__, msg);                                                                                       \
        }                                                                                                                                       \
    } while (0)
