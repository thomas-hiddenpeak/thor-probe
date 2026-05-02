#!/usr/bin/env python3
"""Run thor-probe with timeout and sensible defaults on failure.

Usage:
    python3 examples/run_with_timeout.py [--timeout 60] [--binary /path/to/thor_probe]
"""

import json
import subprocess
import sys
import argparse

DEFAULT_TIMEOUT = 60
DEFAULT_BINARY = None


def run_probe(timeout: int, binary: str = None) -> dict:
    """Run thor_probe and return parsed JSON, or fallback defaults on failure."""
    cmd = [binary] if binary else ["thor_probe"]
    cmd.extend(["--json"])

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
        return json.loads(result.stdout)
    except subprocess.TimeoutExpired:
        print(
            f"WARNING: thor_probe timed out after {timeout}s, using defaults",
            file=sys.stderr,
        )
        return get_defaults()
    except (subprocess.SubprocessError, json.JSONDecodeError) as e:
        print(
            f"WARNING: thor_probe failed ({e}), using defaults", file=sys.stderr
        )
        return get_defaults()


def get_defaults() -> dict:
    return {
        "gpu": {
            "device": {
                "sm_count": 0,
                "global_mem_gb": 0,
                "shared_mem_per_sm": 0,
                "regs_per_sm": 0,
            },
            "tcgen05": None,
            "deep_sm": None,
        },
        "cpu": None,
        "system": None,
        "telemetry": None,
    }


def main():
    parser = argparse.ArgumentParser(description="Run thor-probe with timeout")
    parser.add_argument(
        "--timeout",
        type=int,
        default=DEFAULT_TIMEOUT,
        help=f"Timeout in seconds (default: {DEFAULT_TIMEOUT})",
    )
    parser.add_argument("--binary", type=str, default=None, help="Path to thor_probe binary")
    args = parser.parse_args()

    hw_config = run_probe(args.timeout, args.binary)

    gpu = hw_config.get("gpu", {}) or {}
    device = gpu.get("device", {}) or {}
    sm_count = device.get("sm_count", 0)
    mem_gb = device.get("global_mem_gb", 0)

    print(json.dumps(hw_config, indent=2))
    print(f"\nExtracted: {sm_count} SMs, {mem_gb} GB memory", file=sys.stderr)


if __name__ == "__main__":
    main()
