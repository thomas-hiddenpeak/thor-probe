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

### Text Mode (Full)

```
DeusRidet-Thor Hardware Probe v0.2.0
=====================================
[gpu] Probed: NVIDIA Thor
[tcgen05] Capabilities detected
[deep_sm] Dynamic probes complete
[cpu] Probed: Cortex-X series
[multimedia] Probed
[system] Probed
[telemetry] Snapshot taken

========================================================================
GPU Device Properties
========================================================================
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
  Concurrent Kernels          yes
  Integrated                  yes
  Memory Pool                 yes
  Cooperative Launch          yes
  CUDA Cores/SM               128
  FP32 Units/SM               128
  FP64 Units/SM               2
  INT32 Units/SM              128
  INT8 Units/SM               0
  SASP Units/SM               128
  SFU Units/SM                4
  Warp Schedulers/SM          8
  Texture Units/SM            4
  L1 Cache/SM                 294912 bytes

========================================================================
tcgen05 Tensor Core Capabilities
========================================================================
  nvFP4 (Block Scale)         M16N16K128 (assumed (ptxas sm_110a))
  FP8 E4M3                    M16N16K64
  FP16                        M16N16K16
  BF16                        M16N16K16
  INT8                        M16N16K16
  Tmem Supported              yes
  Tmem Total                  0 KB
  Async Copy TC               yes
  Cluster Launch              yes
  Grid Mem Fence              yes
  Cluster Mem Fence           yes
  Shuffle                     yes
  Vote                        yes

========================================================================
Deep SM Microarchitecture (Dynamic Probes)
========================================================================
  Warp Schedulers/SM              8 (IPC saturation probe)
  Smem Banks                      32 (SM110a crossbar architecture (not measurable))
  Smem Bank Width             32 bits
  L1 Cache/SM                     288 KB (cache inflection probe)
  Max Regs/Thread                 256 (regsPerSM / observedMaxWarps)
  Max Shared/Block            227 KB
  Max Registers/SM                65536 (cudaDeviceProp::regsPerMultiprocessor)
  Max Shared/SM               228 KB
  Max Threads/SM                  1536 (cudaDeviceProp::maxThreadsPerMultiProcessor)

========================================================================
CPU
========================================================================
  Architecture                aarch64
  Model                       Cortex-X series
  Core Count                  14
  Physical IDs                1
  L1d/Core                    64 KB
  L1i/Core                    64 KB
  L2/Cluster                  1024 KB
  L3 Total                    0 KB

========================================================================
Multimedia
========================================================================
  NVENC Status                sdk_unavailable
  NVENC H.264                 yes
  NVENC HEVC                  yes
  NVENC AV1                   no
  NVENC Clock                 0 MHz
  NVDEC Status                sdk_unavailable
  NVDEC Clock                 0 MHz
  NVDEC Codecs                yes
    - H264
    - HEVC
    - AV1
    - VP9
  NVJPEG Status               available
  PVA Status                  sdk_unavailable
  PVA Clock                   0 MHz
  PVA INT8 GMAC/s             2488.00
  PVA FP16 GFLOPS/s           622.00
  OFA Status                  sdk_unavailable
  OFA Optical Flow            no
  OFA Stereo                  no
  ISP0 Status                 not_found
  ISP1 Status                 not_found
  VIC Status                  available

========================================================================
System
========================================================================
  Memory Type                 LPDDR5X
  Bus Width                   256 bits
  Peak BW                     273 GB/s
  Total Memory                122 GB
  Display Outputs             0
  Network Interfaces          15
    - can1 (unknown)
    - mgbe1_0 (10000 Mb/s)
    - usb1 (unknown)
    - wlP1p1s0 (unknown)
    - mgbe0_0 (10000 Mb/s)
    - can2 (unknown)
    - mgbe3_0 (10000 Mb/s)
    - docker0 (-1 Mb/s)
    - can0 (unknown)
    - lo (unknown)
    - enP2p1s0 (1000 Mb/s)
    - mgbe2_0 (10000 Mb/s)
    - usb0 (unknown)
    - can3 (unknown)
    - l4tbr0 (-1 Mb/s)
  PCIe Version                Gen5
    - 0001:00:00.0 (speed=5, width=1)
    - 0005:01:00.0 (speed=16, width=4)
    - 0005:00:00.0 (speed=16, width=4)
    - 0000:01:00.0 (speed=2, width=1)
    - 0002:01:00.0 (speed=8, width=1)
    - 0000:00:00.0 (speed=0, width=0)
    - 0002:00:00.0 (speed=8, width=1)
    - 0001:01:00.0 (speed=5, width=1)

========================================================================
Telemetry Snapshot
========================================================================
  GPU Clock                   1575 MHz
  EMC Clock                   0 MHz
  CPU Clock                   0 MHz
  NVENC0 Clock                0 MHz
  NVDEC0 Clock                0 MHz
  VIC Clock                   0 MHz
  PVA Clock                   0 MHz
  OFA Clock                   0 MHz
  GPU Temp                    40 C
  SOC Temp                    0 C
  GPU Power                   4723 mW
  CPU+SOC Power               7868 mW
  EMC BW                      0 Mbps
  RAM Used/Total              55222 / 125772 MB
  Source                      fused
```

> **Note:** Telemetry values (clocks, power, temp) vary per run and depend on device state. `sdk_unavailable` for NVENC/NVDEC/PVA/OFA means the SDK shared libraries are not installed on this system — the probe still reports hardware capabilities from sysfs.

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
