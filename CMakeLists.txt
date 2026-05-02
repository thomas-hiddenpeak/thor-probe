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
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} --expt-relaxed-constexpr")

# Config-specific optimization (Release / RelWithDebInfo only)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -D_FORTIFY_SOURCE=2 -fstack-protector-strong")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -D_FORTIFY_SOURCE=2 -fstack-protector-strong")
set(CMAKE_CUDA_FLAGS_RELEASE "${CMAKE_CUDA_FLAGS_RELEASE} -O3 --use_fast_math -Xptxas -O3")
set(CMAKE_CUDA_FLAGS_RELWITHDEBINFO "${CMAKE_CUDA_FLAGS_RELWITHDEBINFO} -O3 --use_fast_math -Xptxas -O3")

# ============================================================================
# Dependencies
# ============================================================================
find_package(CUDAToolkit REQUIRED)

# ============================================================================
# Communis: common utilities static library
# ============================================================================
set(COMMUNIS_SOURCES
    src/communis/config.cpp
    src/communis/thor.cpp
)
# timeline_logger.cpp depends on sensus — excluded from explicit list above
add_library(communis STATIC ${COMMUNIS_SOURCES})
target_include_directories(communis PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CUDAToolkit_INCLUDE_DIRS}
)
target_link_libraries(communis PRIVATE CUDA::cudart)

# ============================================================================
# Thor Probe (modular architecture)
# ============================================================================
add_subdirectory(src/thor_probe)
