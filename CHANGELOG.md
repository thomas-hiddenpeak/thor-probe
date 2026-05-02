# Changelog

All notable changes to this project will be documented in this file.

This project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

## [1.0.0] - 2026-05-02

### Added

- Modular probe framework with separate libraries for GPU, CPU, multimedia, system, telemetry, and output domains
- GPU device properties detection (compute capability, memory, clocks, SM count)
- Deep SM microarchitecture probe for Blackwell/SM110a
- TCgen05 tensor core detection via CUDA kernels (`tc_probe_cu.cu`, `deep_sm_probe.cu`)
- CPU topology detection for ARM aarch64 (cores, clusters, caches, frequency)
- Multimedia encoder/decoder probes: NVENC, NVDEC, NVJPEG, PVA, OFA, ISP, VIC
- System probes: memory, display, network, PCIe topology
- Telemetry collection via sysfs and tegrastats
- JSON and text output formatters with JSON schema (`probe_schema.h`)
- Spec reference module with nominal data for Thor T5000/T4000
- Measured vs. nominal spec comparison in probe output
- CLI flags: `--samples N` for repeated sampling, `--sustain SEC` for sustained run timing
- JSON Schema validation and timeout example
- CMake install and export targets for downstream integration
- Catch2 unit test suite (48 tests covering communis utilities)
- CTest integration and GitHub Actions CI workflow
- README, sample output, disclaimer, and MIT license

### Changed

- Replaced `--use_fast_math` flag in CMake build for numerical correctness
- Marked CUDA headers as PRIVATE in CMake target dependencies
- Updated `stof` to `stod` in CPU probe for proper double precision
- Switched multimedia probes from `fscanf` to `std::ifstream` for safe file reading
- Hardened probe_main orchestration with proper error propagation and int64_t sizing
- Sanitized system probe outputs for display, network, and PCIe
- Improved error messages and data flow in probe_main

### Fixed

- GPU probe safety: fixed kernel races, CUDA context cleanup, and error code handling
- Hardened `deep_sm_probe`, `device_props`, and `tc_probe_cu` against malformed device state
- Fixed substr bounds checking in CPU probe
- Added overflow protection in telemetry parsing
- Fixed json_writer.h to prevent buffer overflows during serialization
- Hardened communis utilities (config, timing, error handling, resource safety)
- Fixed data loss and error propagation in probe_main orchestration

### Security

- Hardened json_writer.h against buffer overflow during JSON serialization
- Replaced unsafe `fscanf` with bounds-checked `std::ifstream` in multimedia probes
- Added overflow protection to telemetry string parsing
- Sanitized all system probe outputs to prevent injection via untrusted sysfs data
