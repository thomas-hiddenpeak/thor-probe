#!/usr/bin/env bash
# ============================================================================
# thor-probe — One-click installer
# Installs the pre-built binary from GitHub Releases to /usr/local/bin (or
# a user-specified prefix). Target: aarch64 Linux with CUDA 13.0+.
# ============================================================================
set -euo pipefail

# -- Configuration -----------------------------------------------------------
GITHUB_OWNER="thomas-hiddenpeak"
GITHUB_REPO="thor-probe"
BINARY_NAME="thor_probe"
DEFAULT_PREFIX="/usr/local/bin"
MIN_CUDA_MAJOR=13
MIN_CUDA_MINOR=0

# -- Colors ------------------------------------------------------------------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# -- State -------------------------------------------------------------------
TEMP_DIR=""
DOWNLOADED_BINARY=""
PREFIX="${DEFAULT_PREFIX}"

# -- Helpers -----------------------------------------------------------------

usage() {
  cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Install thor-probe from the latest GitHub Release.

Options:
  --prefix PATH   Install to PATH instead of ${DEFAULT_PREFIX}
  --help          Show this help message

Examples:
  sudo $(basename "$0")                     # install to /usr/local/bin
  sudo $(basename "$0") --prefix /opt/bin   # install to /opt/bin
EOF
  exit 0
}

info()    { echo -e "${BLUE}[INFO]${NC}  $*"; }
ok()      { echo -e "${GREEN}[OK]${NC}    $*"; }
warn()    { echo -e "${YELLOW}[WARN]${NC}  $*"; }
fail()    { echo -e "${RED}[FAIL]${NC}  $*" >&2; }

# -- Cleanup trap ------------------------------------------------------------
cleanup() {
  if [[ -n "${TEMP_DIR}" && -d "${TEMP_DIR}" ]]; then
    rm -rf "${TEMP_DIR}"
  fi
}
trap cleanup EXIT INT TERM

# -- Argument parsing --------------------------------------------------------
while [[ $# -gt 0 ]]; do
  case "$1" in
    --help)   usage ;;
    --prefix)
      PREFIX="$2"
      shift 2
      ;;
    *)
      fail "Unknown option: $1"
      usage
      ;;
  esac
done

# -- Platform checks ---------------------------------------------------------

check_arch() {
  local arch
  arch="$(uname -m)"
  if [[ "${arch}" != "aarch64" ]]; then
    fail "Unsupported architecture: ${arch}"
    fail "thor-probe requires aarch64 (ARM64). This is built for NVIDIA Jetson AGX Thor."
    exit 1
  fi
  ok "Architecture: aarch64"
}

check_os() {
  local kernel
  kernel="$(uname -s)"
  if [[ "${kernel}" != "Linux" ]]; then
    fail "Unsupported OS: ${kernel}"
    fail "thor-probe requires Linux."
    exit 1
  fi
  ok "OS: Linux"
}

check_cuda() {
  # Try multiple ways to detect the CUDA runtime version
  local cuda_version=""

  # Method 1: nvcc --version
  if command -v nvcc &>/dev/null; then
    cuda_version="$(nvcc --version 2>/dev/null | grep -oP 'release \K[0-9]+\.[0-9]+' || true)"
  fi

  # Method 2: read from libcuda / driver version via nvidia-smi
  if [[ -z "${cuda_version}" ]] && command -v nvidia-smi &>/dev/null; then
    local driver_str
    driver_str="$(nvidia-smi --query-gpu=driver_version --format=csv,noheader 2>/dev/null || true)"
    if [[ -n "${driver_str}" ]]; then
      info "CUDA toolkit not found via nvcc, but NVIDIA driver detected (${driver_str})"
      # We can't easily extract toolkit version from driver; skip strict check
      ok "NVIDIA driver detected (${driver_str})"
      return 0
    fi
  fi

  # Method 3: check for libcuda.so
  if [[ -z "${cuda_version}" ]]; then
    if ldconfig -p 2>/dev/null | grep -q libcuda.so; then
      ok "CUDA driver library found (libcuda.so)"
      return 0
    fi
  fi

  # Method 4: check for the CUDA runtime library path
  if [[ -z "${cuda_version}" ]] && [[ -f /usr/lib/aarch64-linux-gnu/libcuda.so ]]; then
    ok "CUDA runtime library found"
    return 0
  fi

  # If we got a version from nvcc, validate it
  if [[ -n "${cuda_version}" ]]; then
    local major minor
    major="${cuda_version%%.*}"
    minor="${cuda_version#*.}"
    if [[ "${major}" -lt "${MIN_CUDA_MAJOR}" ]] || \
       { [[ "${major}" -eq "${MIN_CUDA_MAJOR}" ]] && [[ "${minor}" -lt "${MIN_CUDA_MINOR}" ]]; }; then
      fail "CUDA version too old: ${cuda_version} (minimum ${MIN_CUDA_MAJOR}.${MIN_CUDA_MINOR})"
      exit 1
    fi
    ok "CUDA version: ${cuda_version}"
    return 0
  fi

  # If nothing worked, warn but don't fail — the binary may still work
  warn "Could not determine CUDA version. Proceeding anyway."
  warn "Ensure CUDA ${MIN_CUDA_MAJOR}.${MIN_CUDA_MINOR}+ is installed on the target system."
}

# -- Download & Install ------------------------------------------------------

fetch_latest_release() {
  # Query GitHub API for the latest release (not pre-release)
  info "Fetching latest release from GitHub..."
  local api_url="https://api.github.com/repos/${GITHUB_OWNER}/${GITHUB_REPO}/releases/latest"

  local response
  response="$(curl -sS -H "Accept: application/vnd.github.v3+json" "${api_url}")" || {
    fail "Failed to fetch release info from ${api_url}"
    exit 1
  }

  # Extract download URL for our binary asset
  # The release asset name matches BINARY_NAME exactly
  local download_url
  download_url="$(echo "${response}" | grep -oP '"browser_download_url": "\K[^"]*' | grep "${BINARY_NAME}" | head -1)" || {
    fail "Could not find '${BINARY_NAME}' asset in the latest release"
    fail "Make sure the release includes a '${BINARY_NAME}' binary artifact."
    exit 1
  }

  echo "${download_url}"
}

download_binary() {
  local url="$1"
  local dest="$2"

  info "Downloading ${BINARY_NAME}..."
  # Use -L to follow redirects (GitHub uses them for release assets)
  curl -sS -L -o "${dest}" \
    -H "Accept: application/octet-stream" \
    "${url}" || {
    fail "Download failed for ${url}"
    exit 1
  }

  # Make the downloaded file executable
  chmod +x "${dest}"

  # Verify it's a valid ELF binary
  if ! file "${dest}" 2>/dev/null | grep -q "ELF"; then
    fail "Downloaded file is not a valid ELF binary"
    rm -f "${dest}"
    exit 1
  fi

  ok "Downloaded ${BINARY_NAME}"
}

install_binary() {
  local src="$1"
  local dest_dir="$2"
  local dest_path="${dest_dir}/${BINARY_NAME}"

  # Ensure the destination directory exists
  if [[ ! -d "${dest_dir}" ]]; then
    info "Creating directory: ${dest_dir}"
    mkdir -p "${dest_dir}" || {
      fail "Failed to create ${dest_dir}. Try running with sudo."
      exit 1
    }
  fi

  info "Installing ${BINARY_NAME} to ${dest_path}..."
  cp -f "${src}" "${dest_path}" || {
    fail "Failed to copy binary to ${dest_path}"
    exit 1
  }
  chmod +x "${dest_path}"

  ok "Installed to ${dest_path}"
}

verify_install() {
  local installed="${PREFIX}/${BINARY_NAME}"

  # Check it exists and is executable
  if [[ ! -x "${installed}" ]]; then
    fail "Installed binary is not executable at ${installed}"
    exit 1
  fi

  # Run --version or --help to verify it launches
  local version_output
  if version_output=$("${installed}" --version 2>&1); then
    ok "Version check: ${version_output}"
  elif version_output=$("${installed}" --help 2>&1); then
    ok "Binary is functional (version flag not available)"
  else
    warn "Binary exists but could not verify execution (may require CUDA runtime)"
  fi
}

# -- Main --------------------------------------------------------------------

main() {
  echo ""
  echo "=============================================="
  echo "  thor-probe Installer"
  echo "=============================================="
  echo ""

  # 1. Platform checks
  check_arch
  check_os
  check_cuda

  echo ""

  # 2. Prepare temp directory
  TEMP_DIR="$(mktemp -d)"
  DOWNLOADED_BINARY="${TEMP_DIR}/${BINARY_NAME}"

  # 3. Fetch release info & download
  local release_url
  release_url="$(fetch_latest_release)"
  download_binary "${release_url}" "${DOWNLOADED_BINARY}"

  # 4. Install
  install_binary "${DOWNLOADED_BINARY}" "${PREFIX}"

  echo ""

  # 5. Verify
  verify_install

  echo ""
  ok "thor-probe installed successfully!"
  echo ""
  echo "Run it with:"
  echo "  ${BINARY_NAME}           # text output"
  echo "  ${BINARY_NAME} --json    # JSON output"
  echo ""
}

main "$@"
