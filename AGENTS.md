# thor-probe — AGENTS.md

C++20/CUDA hardware probe for NVIDIA Thor (SM110a, Blackwell). MIT-licensed.

## Build

```bash
mkdir -p build && cd build
cmake ..
make
```

Output binary: `build/src/thor_probe/thor_probe`
Requires CUDA 13.0+ and GCC 13+ on aarch64 (Jetson AGX Thor DevKit).

### Critical CMake Details

- **CUDA architecture is forced to `110a`** — `CMAKE_CUDA_ARCHITECTURES` is set with `FORCE` in root `CMakeLists.txt`.
- **`timeline_logger.cpp` is excluded** from the `communis` static library (depends on external `sensus`).
- No `find_package` beyond `CUDAToolkit` — all dependencies are local or CUDA runtime.
- Feature flags (NVENC, NVDEC, VPI, etc.) are CMake `option()` toggles, mostly ON by default.

## Run

```bash
./build/src/thor_probe/thor_probe        # text output (default)
./build/src/thor_probe/thor_probe --json # JSON output
./build/src/thor_probe/thor_probe 1      # target CUDA device 1
```

## Architecture

Two top-level source directories under `src/`:

- **`communis/`** — shared utilities (logging, config, timing, JSON helpers). Built as `libcommunis.a`.
- **`thor_probe/`** — main probe application:
  - `include/` — public headers (`probe_schema.h`, `gpu_probe.h`, etc.)
  - `src/` — implementation organized by domain:
    - `gpu/` — device properties, tcgen05 tensor core probes, deep SM microarchitecture (`.cu` files)
    - `cpu/` — ARM aarch64 CPU topology detection
    - `multimedia/` — NVENC, NVDEC, NVJPEG, PVA, OFA, ISP, VIC probes
    - `system/` — memory, display, network, PCIe
    - `telemetry/` — sysfs, tegrastats, telemetry manager
    - `output/` — JSON serializer, text formatter
    - `probe_main.cpp` — entry point, argument parsing, orchestrates all probes then prints results

7 static libraries are linked into the `thor_probe` executable. Each library exposes a single probe function.

### CUDA Files

Only 2 `.cu` files exist: `tc_probe_cu.cu` and `deep_sm_probe.cu` (both in `src/gpu/`). `tc_probe_cu.cu` is also compiled directly into the executable (not just the `probe_gpu_cu` library).

## No Test Infrastructure

No tests, CI workflows, linters, formatters, or pre-commit hooks exist. Any added testing or linting is greenfield.

## Style Notes

- Headers use `.h` extension (not `.hpp`), even for C++ headers.
- Everything lives in `deusridet::probe` and `deusridet::telemetry` namespaces.
- CMake uses explicit source lists (no GLOB in subdirectory).
- No external dependencies beyond CUDA runtime.
