// ****************************************************************************
//
//                  PicoPad Arcade Launcher — flasher
//
// ****************************************************************************

#ifndef _FLASHER_H
#define _FLASHER_H

#include "../include.h"
#include "uf2_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

// Flash options chosen by the user in the confirmation dialog
typedef struct {
    Bool preserve_launcher;   // skip writes to protected regions (default ON)
    Bool apply_st7789_init;   // Arcade-specific: ST7789 display init patch
    Bool apply_palette_lut;   // Arcade-specific: palette LUT
    Bool apply_cf2;           // Arcade-specific: CF2 config
} FlashOptions;

typedef enum {
    FLASH_OK,
    FLASH_ERR_OPEN,        // file open failed
    FLASH_ERR_INVALID,     // UF2 magic / family mismatch
    FLASH_ERR_TOO_BIG,     // RAM mode: payload exceeds RAM size
    FLASH_ERR_PROTECTED,   // overlap with protected region in non-preserve flow
    FLASH_ERR_WRITE,       // flash write failure
    FLASH_ERR_INTERRUPTED, // user pressed B during flash
} FlashResult;

// Run a UF2 image from RAM. Loads bytes into SRAM at target_addr,
// then jumps to vector table via GoToAppRam. Never returns on success.
FlashResult FlasherRunRam(const char* path, const Uf2Info* info);

// Flash a UF2 image into flash memory with the given options.
// Returns FLASH_OK on success, error code on failure.
// On success, performs a watchdog reset (= never returns).
FlashResult FlasherWriteFlash(const char* path, const Uf2Info* info,
                              const FlashOptions* opts);

// Check whether (addr, size) overlaps any of the protected regions
// (= our boot2, launcher, state, CF2).
Bool FlasherIsProtected(u32 addr, u32 size);

#ifdef __cplusplus
}
#endif

#endif // _FLASHER_H
