# thor-probe

Modular hardware probe for NVIDIA Thor (SM110a, Blackwell).

## Overview

Comprehensive hardware detection tool that probes GPU microarchitecture, CPU topology, multimedia engines, system resources, and telemetry on the NVIDIA Jetson AGX Thor Developer Kit.

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

## Build

```bash
mkdir build && cd build
cmake .. -G Ninja
ninja
```

Requires CUDA 13.0+ and GCC 13+ on aarch64.

## Usage

```bash
./thor_probe                    # text output
./thor_probe --json             # JSON output
```

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
