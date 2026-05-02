# thor-probe

Modular hardware probe for NVIDIA Thor (SM110a, Blackwell).

## Overview

Comprehensive hardware detection tool that probes GPU microarchitecture, CPU topology, multimedia engines, system resources, and telemetry on the NVIDIA Jetson AGX Thor Developer Kit.

> **⚠️ Disclaimer:** Many detection methods in this project are still under active research and exploration. Results may not be fully accurate — especially for dynamic probes (warp scheduler count, L1 cache size, shared memory banks). Treat output as preliminary approximations rather than definitive specifications.

## Modules

- **probe_gpu** — GPU device properties and dynamic SM microarchitecture detection
- **probe_gpu_cu** — CUDA kernel helpers (tcgen05 detection, deep SM probes)
- **probe_cpu** — CPU topology and capabilities (ARM aarch64)
- **probe_multimedia** — NVENC, NVDEC, NVJPEG, PVA, OFA, ISP, VIC
- **probe_system** — Memory, display, network, PCIe
- **probe_telemetry** — sysfs, tegrastats, telemetry manager
- **probe_output** — JSON serializer and text formatter

## Deep SM Microarchitecture Probes

Dynamic detection of SM-level microarchitecture properties:

- **Warp Schedulers** — Detected via IPC saturation probe (aggregate throughput peak)
- **Shared Memory Banks** — SM110a uses crossbar architecture (no traditional bank conflicts)
- **L1 Cache** — Detected via pointer-chasing with cache inflection analysis
- **Occupancy** — Warps/SM and registers/thread from CUDA occupancy API

## Disclaimer

Many detection methods in this project are still exploratory. Results from dynamic probes (warp scheduler count, L1 cache size, occupancy analysis) are best-effort approximations based on microbenchmark heuristics and may not match official specifications. Use this tool for reference and research — not as authoritative hardware documentation.

## Build

```bash
mkdir build && cd build
cmake ..
make
```

Requires CUDA 13.0+ and GCC 13+ on aarch64 (tested on Jetson AGX Thor DevKit).

## Usage

```bash
./build/src/thor_probe/thor_probe       # text output (default)
./build/src/thor_probe/thor_probe --json  # JSON output
```

## Example Output

### Text Mode

```
GPU Device Properties
==================================================
  Name                        NVIDIA Thor
  Compute Capability          11.0
  SM Count                    20
  Global Memory               122 GB
  Shared Mem/SM               228 KB
  Registers/SM                65536
  Warp Size                   32
  Max Threads/Block           1024
  Max Threads/SM              1536
  Total Warps/SM              48
  L2 Cache                    32 MB
  Clock Rate (kHz)            1575000
  Max Clock Rate (kHz)        1575000
  CUDA Cores/SM               128
  Warp Schedulers/SM          8
  Texture Units/SM            4
  L1 Cache/SM                 294912 bytes

Deep SM Microarchitecture (Dynamic Probes)
==================================================
  Warp Schedulers/SM              8 (IPC saturation probe)
  Smem Banks                      32 (SM110a crossbar architecture (not measurable))
  L1 Cache/SM                     288 KB (cache inflection probe)
  Max Regs/Thread                 256 (regsPerSM / observedMaxWarps)
  Max Shared/Block            227 KB
  Max Threads/SM                  1536 (cudaDeviceProp::maxThreadsPerMultiProcessor)

CPU
==================================================
  Architecture                aarch64
  Model                       Cortex-X series
  Core Count                  14
  L1d/Core                    64 KB
  L1i/Core                    64 KB
  L2/Cluster                  1024 KB

Telemetry Snapshot
==================================================
  GPU Clock                   1575 MHz
  GPU Temp                    40 C
  GPU Power                   5116 mW
  CPU+SOC Power               7868 mW
  RAM Used/Total              55220 / 125772 MB
```

### JSON Mode

Run with `--json` flag to get machine-readable output.

## Output

### GPU

Device properties, compute capabilities, tensor core support (nvFP4, FP8, FP16, BF16, INT8), deep SM microarchitecture.

### CPU

Architecture, core count, cache hierarchy, frequency ranges.

### Multimedia

Encoder/decoder status, codec support, clock rates, PVA/OFA capabilities.

### System

Memory type and bandwidth, network interfaces, PCIe topology.

### Telemetry

Real-time GPU/EMC/CPU clocks, temperatures, power consumption.

## Architecture

```
thor-probe/
├── CMakeLists.txt          # Top-level build configuration
├── src/
│   ├── communis/           # Common utilities (logging, config, timing)
│   └── thor_probe/
│       ├── CMakeLists.txt  # Probe module build
│       ├── include/        # Public headers
│       └── src/
│           ├── cpu/        # CPU detection
│           ├── gpu/        # GPU + deep SM probes
│           ├── multimedia/ # NVENC/NVDEC/NVJPEG/PVA/OFA/ISP/VIC
│           ├── system/     # Memory/display/network/PCIe
│           ├── telemetry/  # sysfs/tegrastats
│           └── output/     # JSON/text formatters
```

## License

MIT — see [LICENSE](LICENSE).
