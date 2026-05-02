cmake_minimum_required(VERSION 3.24 FATAL_ERROR)

# ============================================================================
# Catch2 via FetchContent (v3.4.0) — zero external dependencies
# ============================================================================
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY  https://github.com/catchorg/Catch2.git
    GIT_TAG         v3.4.0
)
FetchContent_MakeAvailable(Catch2)

# ============================================================================
# Test sources — pure C++ only (no CUDA/hardware dependency)
# ============================================================================
set(TEST_SOURCES
    test_json_writer.cpp
    test_config.cpp
    test_spec_reference.cpp
    test_ring_buffer.cpp
    test_tempus.cpp
)

add_executable(thor_probe_tests ${TEST_SOURCES})
target_link_libraries(thor_probe_tests PRIVATE communis Catch2::Catch2WithMain)
target_compile_features(thor_probe_tests PRIVATE cxx_std_20)

# Exclude from install
set_target_properties(thor_probe_tests PROPERTIES
    INSTALL_EXCLUDE_FROM_ALL ON
)

# ============================================================================
# CTest integration
# ============================================================================
include(CTest)
list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
include(Catch)
catch_discover_tests(thor_probe_tests)
