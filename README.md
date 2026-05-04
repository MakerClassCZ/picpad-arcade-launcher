# PicoPad Arcade Launcher

Custom launcher firmware for the [Pajenicko PicoPad](https://picopad.eu) — a handheld
RP2040 game console. The launcher boots first, auto-runs the previously flashed
MakeCode Arcade game on a watchdog reset, or shows a menu where you can flash a
new game from the SD card with on-the-fly ST7789 display patching and palette
remapping.

> Why this exists: stock MakeCode Arcade only supports ILI9341 / ST7735 displays.
> PicoPad uses ST7789. Drag-and-dropping any Arcade game gives you a black screen.
> This launcher patches the runtime binary at flash time so unmodified Arcade UF2s
> just work.

## Features

- **Auto-launch** the flashed game after a watchdog reset
- **Hard reset / power-on** → menu (PLAY / LOAD / DEBUG SHELL / USB MSC / BOOTSEL)
- **PLAY** detects what is in flash and dispatches:
  - Arcade game — jumps to `0x10000100` (Arcade vector table)
  - PicoLibSDK app — auto-detected via PPAD magic, jumps to `0x10008000`
  - PicoLibSDK loader without app — jumps to loader, skips BootScreenSaver via
    `WATCHDOG_SCRATCH[4]` magic
- **LOAD from SD** — file picker over the SD card, UF2 selection dialog with
  toggleable patches, flash with progress bar
- **USB MSC** — exposes the SD card as a USB drive (drag-drop file transfer
  between PC and SD)
- **DEBUG SHELL** — interactive shell over USB CDC (`COMx` / `/dev/ttyACMx`)
  with commands: `dump`, `scan`, `readraw`, `readuf2`, `verify`, `flash`,
  `lut`, `cf2`, `bootsel`, `reset`, `exit`
- **REBOOT TO BOOTSEL** — drag-drop a UF2 from the host

Patches applied at flash time (all use **full-region signature scan** so they
survive across MakeCode runtime versions whose offsets shift between builds):

- **ST7789 init** replaces the ILI9341 init sequence (signature
  `EF 03 03 80 02 CF 03`)
- **Palette stub** + 256-byte LUT at `0x10080000` (signature
  `4B 01 0B 43 CC 10 04 43`)
- **CF2 config** at `0x100FF000` — PicoPad-specific configuration block

## Repository layout

```
picopad-arcade-launcher/
├── README.md
├── setup_env.sh / setup_env.bat   ← export PICO_ROOT_PATH
├── launcher/                      ← the launcher firmware
│   ├── c.sh                       ← build script (calls picolibsdk/_c1.sh)
│   ├── config.h                   ← compile-time feature flags
│   ├── Makefile
│   ├── launcher_m1.ld             ← 1 MB layout (CIRCUITPY-safe, default)
│   ├── launcher_m1_2mb.ld         ← 2 MB layout (overwrites CIRCUITPY)
│   ├── bin_to_uf2.py              ← BIN → UF2 packer (called by c.sh)
│   ├── post_process_boot2.py      ← patches boot2 jump target + CRC32
│   └── src/                       ← C++ sources
└── picolibsdk/                    ← bundled PicoLibSDK (pruned to picopad10)
```

`picolibsdk/` is a vendored, pruned copy of [Panda381/PicoLibSDK](https://github.com/Panda381/PicoLibSDK).
Sample programs and other-device support directories were dropped, leaving the
core SDK + picopad10 device files. One file is patched relative to upstream:
`_sdk/usb_src/sdk_usb_dev.c` exposes the MSC class driver entry that upstream
keeps in `_sdk/usb_src/TODO/`.

## Build prerequisites

- **GCC ARM Embedded 12.x or newer** (`gcc-arm-none-eabi`)
  - Linux: `apt install gcc-arm-none-eabi`
  - macOS: `brew install --cask gcc-arm-embedded`
  - Windows: [ARM GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
  - The `setup_env` scripts honour `ARM_GCC_DIR` if you need to pin a
    specific toolchain (= when multiple are installed). Otherwise whichever
    `arm-none-eabi-gcc` is first on PATH is used.
- **GNU Make** — bundled inside `picolibsdk/_tools/` and added to PATH by
  `setup_env.sh` / `setup_env.bat` automatically
- **Python 3.8+** for the post-processing scripts in `launcher/` (no
  third-party packages required)

GCC 10.x is **not enough** — the linker flag `--no-warn-rwx-segment` and modern
address expressions need binutils 2.39+.

> **Windows users:** ARM GCC cannot handle non-ASCII characters anywhere in the
> path. If your username (= `%USERPROFILE%`) contains any accented characters
> the build will fail with cryptic `cc1.exe: fatal error ... No such file or
> directory` errors. Clone this repo to an ASCII-only path such as
> `C:\projects\picopad-arcade-launcher` (or set up a directory junction with
> `mklink /J`).

## Build

```bash
# Linux / macOS / Git Bash on Windows
cd picopad-arcade-launcher
source setup_env.sh
cd launcher
./c.sh                 # picopad10 (default), 1 MB layout
```

```cmd
:: Windows cmd
cd picopad-arcade-launcher
setup_env.bat
cd launcher
bash c.sh
```

Output:

- `ARCADELAUNCHER_install.uf2` — full layout including boot2; flash this onto a
  blank PicoPad in BOOTSEL mode (drag-drop). **Wipes any flashed game.**
- `ARCADELAUNCHER_update.uf2` — launcher region only (no boot2). Use this to
  update the launcher while keeping the currently flashed game intact.

### 2 MB flash layout (no CIRCUITPY)

Default layout occupies the first 1 MB of flash, leaving the second 1 MB
untouched so a CircuitPython `CIRCUITPY` filesystem (at `0x10100000+`)
survives. If you don't need CIRCUITPY and want maximum game space:

```bash
EXTRA_CFLAGS="-DLAUNCHER_2MB=1" ./c.sh
```

This puts the launcher at `0x101F0000` and lets games take ~1.95 MB instead of
~960 KB.

### Release build (no debug shell, no USB MSC)

```bash
EXTRA_CFLAGS="-DLAUNCHER_RELEASE" ./c.sh
```

Disables both the USB CDC debug shell and the USB MSC SD reader. Saves ~25 KB
and is what you ship to non-developer users.

## Usage

1. Flash `ARCADELAUNCHER_install.uf2` onto your PicoPad in BOOTSEL mode.
2. Put MakeCode Arcade `.uf2` games onto an SD card (any folder layout).
3. Insert the SD card, power-cycle the PicoPad — the launcher menu appears.
4. **LOAD from SD** → pick a game → confirm patches → wait for flash to finish.
5. The PicoPad reboots into the game. From now on, watchdog resets bring you
   straight back to the game; pressing the hardware RESET button gets you to
   the launcher menu.

## Credits

- [Panda381/PicoLibSDK](https://github.com/Panda381/PicoLibSDK) — the alternative
  RP2040/RP2350 SDK that this project builds on
- [Pajenicko PicoPad](https://github.com/pajenicko/picopad) — the hardware
- [microsoft/uf2](https://github.com/microsoft/uf2) — UF2 format spec and tools
- [microsoft/pxt-arcade](https://github.com/microsoft/pxt-arcade) — MakeCode
  Arcade runtime
