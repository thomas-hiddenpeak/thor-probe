# ==============================================================================
# thor_probe — Modular hardware probe for SM110a (Blackwell / NVIDIA Thor)
# ==============================================================================

set(PROBE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(PROBE_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

# ==============================================================================
# 1. probe_gpu — GPU device properties (CPU-side)
# ==============================================================================
add_library(probe_gpu STATIC
    "${PROBE_SRC_DIR}/gpu/device_props.cpp"
)
target_include_directories(probe_gpu PUBLIC
    $<BUILD_INTERFACE:${PROBE_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:include/thor_probe>
)
target_include_directories(probe_gpu PRIVATE
    "${PROBE_SRC_DIR}"
)
target_compile_features(probe_gpu PUBLIC cxx_std_20)
target_compile_options(probe_gpu PRIVATE -Wall -Wextra)
target_link_libraries(probe_gpu PUBLIC communis)

# ==============================================================================
# 2. probe_gpu_cu — CUDA kernel helpers (TC probe, etc.)
# ==============================================================================
add_library(probe_gpu_cu STATIC
    "${PROBE_SRC_DIR}/gpu/tc_probe_cu.cu"
    "${PROBE_SRC_DIR}/gpu/deep_sm_probe.cu"
)
target_include_directories(probe_gpu_cu PUBLIC
    $<BUILD_INTERFACE:${PROBE_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:include/thor_probe>
)
target_include_directories(probe_gpu_cu PRIVATE
    "${PROBE_SRC_DIR}"
)
target_compile_options(probe_gpu_cu PRIVATE -Xcompiler -Wall,-Wextra)
target_link_libraries(probe_gpu_cu PUBLIC communis)

# ==============================================================================
# 3. probe_multimedia — NVENC, NVDEC, NVJPEG, PVA, OFA, ISP, VIC
# ==============================================================================
add_library(probe_multimedia STATIC
    "${PROBE_SRC_DIR}/multimedia/nvenc.cpp"
    "${PROBE_SRC_DIR}/multimedia/nvdec.cpp"
    "${PROBE_SRC_DIR}/multimedia/nvjpeg.cpp"
    "${PROBE_SRC_DIR}/multimedia/pva.cpp"
    "${PROBE_SRC_DIR}/multimedia/ofa.cpp"
    "${PROBE_SRC_DIR}/multimedia/isp.cpp"
    "${PROBE_SRC_DIR}/multimedia/vic.cpp"
)
target_include_directories(probe_multimedia PUBLIC
    $<BUILD_INTERFACE:${PROBE_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:include/thor_probe>
)
target_include_directories(probe_multimedia PRIVATE
    "${PROBE_SRC_DIR}"
)
target_compile_features(probe_multimedia PUBLIC cxx_std_20)
target_compile_options(probe_multimedia PRIVATE -Wall -Wextra)
target_link_libraries(probe_multimedia PRIVATE communis)

# ==============================================================================
# 4. probe_cpu — CPU topology and capabilities
# ==============================================================================
add_library(probe_cpu STATIC
    "${PROBE_SRC_DIR}/cpu/cpu_probe.cpp"
)
target_include_directories(probe_cpu PUBLIC
    $<BUILD_INTERFACE:${PROBE_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:include/thor_probe>
)
target_include_directories(probe_cpu PRIVATE
    "${PROBE_SRC_DIR}"
)
target_compile_features(probe_cpu PUBLIC cxx_std_20)
target_compile_options(probe_cpu PRIVATE -Wall -Wextra)
target_link_libraries(probe_cpu PRIVATE communis)

# ==============================================================================
# 5. probe_system — Memory, display, network, PCIe
# ==============================================================================
add_library(probe_system STATIC
    "${PROBE_SRC_DIR}/system/memory.cpp"
    "${PROBE_SRC_DIR}/system/display.cpp"
    "${PROBE_SRC_DIR}/system/network.cpp"
    "${PROBE_SRC_DIR}/system/pcie.cpp"
)
target_include_directories(probe_system PUBLIC
    $<BUILD_INTERFACE:${PROBE_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:include/thor_probe>
)
target_include_directories(probe_system PRIVATE
    "${PROBE_SRC_DIR}"
)
target_compile_features(probe_system PUBLIC cxx_std_20)
target_compile_options(probe_system PRIVATE -Wall -Wextra)
target_link_libraries(probe_system PRIVATE communis)

# ==============================================================================
# 6. probe_telemetry — sysfs, tegrastats, telemetry manager
# ==============================================================================
add_library(probe_telemetry STATIC
    "${PROBE_SRC_DIR}/telemetry/sysfs_source.cpp"
    "${PROBE_SRC_DIR}/telemetry/tegrastats_source.cpp"
    "${PROBE_SRC_DIR}/telemetry/telemetry_manager.cpp"
)
target_include_directories(probe_telemetry PUBLIC
    $<BUILD_INTERFACE:${PROBE_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:include/thor_probe>
)
target_include_directories(probe_telemetry PRIVATE
    "${PROBE_SRC_DIR}"
)
target_compile_features(probe_telemetry PUBLIC cxx_std_20)
target_compile_options(probe_telemetry PRIVATE -Wall -Wextra)
target_link_libraries(probe_telemetry PRIVATE communis)

# ==============================================================================
# 7. probe_output — JSON serializer and text formatter
# ==============================================================================
add_library(probe_output STATIC
    "${PROBE_SRC_DIR}/output/json_serializer.cpp"
    "${PROBE_SRC_DIR}/output/text_formatter.cpp"
)
target_include_directories(probe_output PUBLIC
    $<BUILD_INTERFACE:${PROBE_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:include/thor_probe>
)
target_include_directories(probe_output PRIVATE
    "${PROBE_SRC_DIR}"
)
target_compile_features(probe_output PUBLIC cxx_std_20)
target_compile_options(probe_output PRIVATE -Wall -Wextra)
target_link_libraries(probe_output PRIVATE communis)

# ==============================================================================
# Feature flags
# ==============================================================================
option(THOR_PROBE_ENABLE_NVENC  "Enable NVENC probe" ON)
option(THOR_PROBE_ENABLE_NVDEC  "Enable NVDEC probe" ON)
option(THOR_PROBE_ENABLE_NVJPEG "Enable NVJPEG probe" ON)
option(THOR_PROBE_ENABLE_PVA    "Enable PVA probe" ON)
option(THOR_PROBE_ENABLE_OFA    "Enable OFA probe" ON)

# Apply feature flags to multimedia probe
target_compile_definitions(probe_multimedia PRIVATE
    $<$<BOOL:${THOR_PROBE_ENABLE_NVENC}>:THOR_PROBE_ENABLE_NVENC>
    $<$<BOOL:${THOR_PROBE_ENABLE_NVDEC}>:THOR_PROBE_ENABLE_NVDEC>
    $<$<BOOL:${THOR_PROBE_ENABLE_NVJPEG}>:THOR_PROBE_ENABLE_NVJPEG>
    $<$<BOOL:${THOR_PROBE_ENABLE_PVA}>:THOR_PROBE_ENABLE_PVA>
    $<$<BOOL:${THOR_PROBE_ENABLE_OFA}>:THOR_PROBE_ENABLE_OFA>
)

# ==============================================================================
# Executables
# ==============================================================================
add_executable(thor_probe "${PROBE_SRC_DIR}/probe_main.cpp")
target_include_directories(thor_probe PRIVATE
    ${PROBE_INCLUDE_DIR}
    ${PROBE_SRC_DIR}
)
target_link_libraries(thor_probe PRIVATE
    probe_gpu
    probe_gpu_cu
    probe_cpu
    probe_multimedia
    probe_system
    probe_telemetry
    probe_output
    communis
)

# ==============================================================================
# Spec reference model (for measured vs nominal comparison)
# ==============================================================================
set(THOR_PROBE_SPEC_MODEL "None" CACHE STRING "Spec reference model: T5000, T4000, or None (default: None)")
set_property(CACHE THOR_PROBE_SPEC_MODEL PROPERTY STRINGS "None" "T5000" "T4000")

# Apply to thor_probe executable
if(THOR_PROBE_SPEC_MODEL STREQUAL "T5000")
  target_compile_definitions(thor_probe PRIVATE THOR_PROBE_SPEC_MODEL_T5000)
elseif(THOR_PROBE_SPEC_MODEL STREQUAL "T4000")
  target_compile_definitions(thor_probe PRIVATE THOR_PROBE_SPEC_MODEL_T4000)
endif()

# ==============================================================================
# Install all thor_probe targets
# ==============================================================================
install(TARGETS probe_gpu probe_gpu_cu probe_multimedia probe_cpu probe_system probe_telemetry probe_output thor_probe
        EXPORT thor-probe-targets
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
)

# Package config
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/thor-probe-config-version.cmake"
    VERSION "0.2.0"
    COMPATIBILITY SameMajorVersion
)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/thor-probe-config.cmake"
"include(\"\${CMAKE_CURRENT_LIST_DIR}/thor-probe-targets.cmake\")\n"
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/thor-probe-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/thor-probe-config-version.cmake"
    DESTINATION lib/cmake/thor-probe
)
install(EXPORT thor-probe-targets
        FILE thor-probe-targets.cmake
        NAMESPACE deusridet::
        DESTINATION lib/cmake/thor-probe
)
