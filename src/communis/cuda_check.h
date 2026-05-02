#pragma once
#include <cuda_runtime.h>
#include <stdexcept>
#include <string>
#include "log.h"

#define cudaCheck(err)                                                                                                    \
    do {                                                                                                                              \
        cudaError_t status = (err);                                                                                                   \
        if (status != cudaSuccess) {                                                                                                  \
            LOG_ERROR("CUDA", "cudaCheck failed at %s:%d: %s",                                                                        \
                      __FILE__, __LINE__, cudaGetErrorString(status));                                                                \
            throw std::runtime_error(std::string("CUDA error: ") + cudaGetErrorString(status));                          \
        }                                                                                                                                       \
    } while (0)
