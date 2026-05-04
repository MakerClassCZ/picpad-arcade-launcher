#!/usr/bin/env bash
# Build script for PicoPad Arcade Launcher
#   Usage:  ./c.sh [picopad10|picopad20|picopad20riscv|picopadhstx|picopadhstxriscv]
#   Default device when no arg: picopad10 (RP2040 PicoPad)

set -e

# Default to picopad10 if no argument
DEVICE="${1:-picopad10}"

# Sanity check: PICO_ROOT_PATH must be set
if [ -z "$PICO_ROOT_PATH" ]; then
    echo "ERROR: PICO_ROOT_PATH not set."
    echo "Run: source ../setup_env.sh"
    exit 1
fi

# PicoLibSDK build conventions
export TARGET="ARCADELAUNCHER"
export GRPDIR="ARCADE"
export MEMMAP=""

# Enable LTO (link-time optimization) by default. Massively reduces UF2 size
# (= ~13 KB / 12% on dev build) by stripping cross-unit dead code (e.g.
# FontBold8x16 + variant SelFont* helpers we never call).
export EXTRA_CFLAGS="${EXTRA_CFLAGS:-} -flto"

# Flash layout: 1 MB (default, CIRCUITPY-safe) or 2 MB (= -DLAUNCHER_2MB=1
# in EXTRA_CFLAGS). Pick matching boot2 jump target + bin_to_uf2 skip.
case "${EXTRA_CFLAGS:-}" in
    *-DLAUNCHER_2MB=1*)
        LAUNCHER_BASE_ADDR="0x101F0000"
        LAYOUT_LABEL="2 MB"
        ;;
    *)
        LAUNCHER_BASE_ADDR="0x100F0000"
        LAYOUT_LABEL="1 MB"
        ;;
esac
echo "Layout: $LAYOUT_LABEL — launcher at $LAUNCHER_BASE_ADDR"

# Invoke PicoLibSDK's _c1.sh. The script always exits non-zero (=
# PicoPadLoaderCrc step fails on our custom layout), so plain `|| true` masks
# real compile errors. Capture output, check for actual compile/link errors
# before proceeding.
build_log=$("$PICO_ROOT_PATH/_c1.sh" "$DEVICE" 2>&1) || true
echo "$build_log"

# Real compilation errors trip GCC's "error:" or "fatal error:" output.
# Linker errors typically have "undefined reference" or "ld returned".
if echo "$build_log" | grep -qE 'error:|fatal error|undefined reference|ld returned [1-9]'; then
    echo "ERROR: compilation/link failed — see build log above"
    rm -f ARCADELAUNCHER.bin  # avoid stale cache fooling next build
    exit 1
fi

# Verify the bin was produced
if [ ! -f "ARCADELAUNCHER.bin" ]; then
    echo "ERROR: ARCADELAUNCHER.bin not produced"
    exit 1
fi

# Post-process M1 binary
echo
echo "=== M1 post-processing ==="
python ./post_process_boot2.py ARCADELAUNCHER.bin --target "$LAUNCHER_BASE_ADDR"

# "install" UF2 — full layout including boot2 (= sector 0). For initial
# install on a blank PicoPad. Reflashing this WILL erase any flashed game.
python ./bin_to_uf2.py ARCADELAUNCHER.bin ARCADELAUNCHER_install.uf2

# "update" UF2 — only launcher region. For updating launcher without losing
# the currently flashed game. Skips boot2 sector 0 entirely.
python ./bin_to_uf2.py ARCADELAUNCHER.bin ARCADELAUNCHER_update.uf2 \
    --skip-below "$LAUNCHER_BASE_ADDR"

echo
echo "=== Build complete ==="
ls -la ARCADELAUNCHER_install.uf2 ARCADELAUNCHER_update.uf2
