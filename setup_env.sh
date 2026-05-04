#!/usr/bin/env bash
# Source this file to set PICO_ROOT_PATH for the launcher build.
#   Usage:  source setup_env.sh
#
# Defaults to the bundled PicoLibSDK copy under ./picolibsdk/.
# Override by exporting PICO_ROOT_PATH before sourcing this file.

# Resolve absolute path to the directory containing this script.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"

if [ -z "$PICO_ROOT_PATH" ]; then
    export PICO_ROOT_PATH="${SCRIPT_DIR}/picolibsdk"
fi

if [ ! -d "$PICO_ROOT_PATH" ]; then
    echo "ERROR: PicoLibSDK not found at $PICO_ROOT_PATH"
    return 1 2>/dev/null || exit 1
fi

echo "PICO_ROOT_PATH = $PICO_ROOT_PATH"

# Add PicoLibSDK tools (make.exe, PicoPadLoaderCrc, etc.) to PATH.
if [ -d "${PICO_ROOT_PATH}/_tools" ]; then
    export PATH="${PICO_ROOT_PATH}/_tools:$PATH"
fi

# If ARM_GCC_DIR is set, prepend it to PATH (= explicit override, e.g. when
# multiple toolchains are installed). Otherwise the existing PATH is used.
# Build requires binutils 2.39+ (= GCC ARM 12.x or newer).
if [ -n "$ARM_GCC_DIR" ]; then
    if [ -x "${ARM_GCC_DIR}/arm-none-eabi-gcc" ] || [ -x "${ARM_GCC_DIR}/arm-none-eabi-gcc.exe" ]; then
        export PATH="${ARM_GCC_DIR}:$PATH"
        echo "ARM toolchain   = ${ARM_GCC_DIR}"
    else
        echo "WARNING: ARM_GCC_DIR set but arm-none-eabi-gcc not found there."
    fi
fi

if ! command -v arm-none-eabi-gcc >/dev/null 2>&1; then
    echo "WARNING: arm-none-eabi-gcc not found on PATH."
    echo "  Install: https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads"
    echo "  Then add its bin/ directory to PATH or export ARM_GCC_DIR."
fi
