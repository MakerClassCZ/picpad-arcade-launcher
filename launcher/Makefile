# PicoPad Arcade Launcher Makefile
# ------------------------------------------------------------
# Requires PICO_ROOT_PATH env var pointing at a PicoLibSDK root.
# By default the bundled copy under ../picolibsdk/ is used (set by
# setup_env.sh / setup_env.bat). Override the env var to point at
# a different SDK install if needed.
# ------------------------------------------------------------

# ASM source files
# boot2_picopad.S = local copy of PicoLibSDK boot2_w25q080_RP2040_bin.S.
# Adding it here BEFORE include ensures Makefile.inc's $(foreach $(eval ...))
# generates a build rule for it.
ASRC += src/boot2_picopad.S

# C source files
CSRC +=

# USB MSC source is vendored into src/ (= copy from PicoLibSDK
# _sdk/usb_src/TODO/sdk_usb_dev_msc.c with adjusted include paths). Compiled
# unless we're building a release (= -DLAUNCHER_RELEASE which disables MSC
# in config.h).
ifeq ($(findstring -DLAUNCHER_RELEASE,$(EXTRA_CFLAGS)),)
  CSRC += src/sdk_usb_dev_msc.c
endif

# C++ source files
SRC += src/main.cpp
SRC += src/menu.cpp
SRC += src/file_picker.cpp
SRC += src/uf2_parser.cpp
SRC += src/flash_dialog.cpp
SRC += src/flasher.cpp
SRC += src/arcade_patches.cpp
SRC += src/debug_shell.cpp
ifeq ($(findstring -DLAUNCHER_RELEASE,$(EXTRA_CFLAGS)),)
  SRC += src/usb_msc_screen.cpp
endif

# Path to PicoLibSDK root (override via env var or set explicitly)
ifeq ($(PICO_ROOT_PATH),)
$(error PICO_ROOT_PATH not set. Source ../setup_env.sh first or set PICO_ROOT_PATH manually.)
endif

# Makefile includes
include ${PICO_ROOT_PATH}/Makefile.inc

# ------------------------------------------------------------
# Compatibility shims — applied AFTER include so they override
# the defaults from PicoLibSDK Makefile.inc.
# ------------------------------------------------------------

# Older ARM GCC (10.x) doesn't support --no-warn-rwx-segment flag.
# This flag is harmless (just suppresses a warning), so drop it
# if user has older toolchain. Using subst because filter-out has
# trouble with the comma in -Wl,--option syntax.
LDFLAGS := $(subst -Wl$(strip ,)--no-warn-rwx-segment,,$(LDFLAGS))

# ------------------------------------------------------------
# M1: Override linker script for custom flash layout
# ------------------------------------------------------------
# Default PicoLibSDK linker scripts place app at 0x10000100 (default.ld) or
# 0x10008000 (app.ld with boot3 loader). We need launcher at end of flash
# with custom boot2 jumping there. launcher_m1.ld = 1 MB layout (default,
# CIRCUITPY-safe). launcher_m1_2mb.ld = 2 MB layout (= -DLAUNCHER_2MB=1).
# Use relative path to avoid non-ASCII chars in absolute $(CURDIR)
# (older binutils ld can't open UTF-8 paths on Windows).
ifneq ($(findstring -DLAUNCHER_2MB=1,$(EXTRA_CFLAGS)),)
  LDSCRIPT := launcher_m1_2mb.ld
else
  LDSCRIPT := launcher_m1.ld
endif

# LDFLAGS in Makefile.inc is a simple variable, so $(LDSCRIPT) was expanded
# at the time of `LDFLAGS += -Wl,-script=$(LDSCRIPT)` (line 348). Override
# now requires substituting the full path of the default script with ours.
LDFLAGS := $(patsubst -Wl$(strip ,)-script=%,-Wl$(strip ,)-script=$(LDSCRIPT),$(LDFLAGS))

# PicoLibSDK adds loader_picopad10.S (= 32 KB boot3 loader) to ASRC for
# DEVCLASS=picopad. We don't want that — our launcher replaces it.
ASRC := $(filter-out %loader_picopad10.S,$(ASRC))
ASRC := $(filter-out %loader_picopad08.S,$(ASRC))
ASRC := $(filter-out %loader_picopad20.S,$(ASRC))
ASRC := $(filter-out %loader_picopadhstx.S,$(ASRC))
ASRC := $(filter-out %loader_picopadvga.S,$(ASRC))
