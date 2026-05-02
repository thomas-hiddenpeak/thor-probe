cmake_minimum_required(VERSION 3.24 FATAL_ERROR)

# thor-probe: NVIDIA Thor (SM110a) hardware probe standalone tool
project(thor-probe LANGUAGES C CXX CUDA)

# ============================================================================
# Build standards
# ============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CUDA_STANDARD 20)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

# ============================================================================
# CUDA architecture: SM110a (Blackwell / NVIDIA Thor)
# ============================================================================
set(CMAKE_CUDA_ARCHITECTURES "110a" CACHE STRING "CUDA architectures" FORCE)
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} \
    --expt-relaxed-constexpr \
    -O3 --use_fast_math \
    -Xptxas -O3")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

# ============================================================================
# Dependencies
# ============================================================================
find_package(CUDAToolkit REQUIRED)

# ============================================================================
# Include directories
# ============================================================================
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CUDAToolkit_INCLUDE_DIRS}
)

# ============================================================================
# Communis: common utilities static library
# ============================================================================
file(GLOB_RECURSE COMMUNIS_SOURCES "src/communis/*.cpp")
# timeline_logger.cpp depends on sensus — exclude
list(FILTER COMMUNIS_SOURCES EXCLUDE REGEX "timeline_logger\\.cpp$")
add_library(communis STATIC ${COMMUNIS_SOURCES})
target_link_libraries(communis PRIVATE CUDA::cudart)

# ============================================================================
# Thor Probe (modular architecture)
# ============================================================================
add_subdirectory(src/thor_probe)
