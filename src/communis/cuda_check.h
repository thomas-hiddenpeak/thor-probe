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
