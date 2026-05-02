cmake_minimum_required(VERSION 3.24 FATAL_ERROR)

# thor-probe: NVIDIA Thor (SM110a) hardware probe standalone tool
project(thor-probe LANGUAGES CXX CUDA)

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

# Config-specific optimization (Release / RelWithDebInfo only)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -D_FORTIFY_SOURCE=2 -fstack-protector-strong")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -D_FORTIFY_SOURCE=2 -fstack-protector-strong")
set(CMAKE_CUDA_FLAGS_RELEASE "${CMAKE_CUDA_FLAGS_RELEASE} -O3 -Xptxas -O3")
set(CMAKE_CUDA_FLAGS_RELWITHDEBINFO "${CMAKE_CUDA_FLAGS_RELWITHDEBINFO} -O3 -Xptxas -O3")

# ============================================================================
# Dependencies
# ============================================================================
find_package(CUDAToolkit REQUIRED)

if(CMAKE_CUDA_COMPILER_VERSION VERSION_LESS "13.0")
  message(FATAL_ERROR "thor-probe requires CUDA 13.0 or later (found ${CMAKE_CUDA_COMPILER_VERSION})")
endif()

# ============================================================================
# Communis: common utilities static library
# ============================================================================
set(COMMUNIS_SOURCES
    src/communis/config.cpp
    src/communis/spec_reference.cpp
    src/communis/thor.cpp
)
# timeline_logger.cpp depends on sensus — excluded from explicit list above
add_library(communis STATIC ${COMMUNIS_SOURCES})
target_include_directories(communis PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_include_directories(communis PRIVATE
    ${CUDAToolkit_INCLUDE_DIRS}
)
target_link_libraries(communis PUBLIC CUDA::cudart)
target_compile_options(communis PRIVATE
  $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra>
  $<$<COMPILE_LANGUAGE:CUDA>:-Wall>
)

# ============================================================================
# Thor Probe (modular architecture)
# ============================================================================
add_subdirectory(src/thor_probe)
