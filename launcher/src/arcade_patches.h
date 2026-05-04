// ****************************************************************************
//
//      PicoPad Arcade Launcher — patch payloads for MakeCode Arcade UF2
//
//   Three Arcade-specific patches applied during flash streaming:
//     1. ILI9341 init signature (7 B) → ST7789 init bytes (23 B in 114 B slot)
//     2. ENC16 palette loop signature (8 B) → palette LUT copy stub (30 B)
//     3. Arcade default palette LUT written to 0x10080000 (64 B)
//   Plus CF2 config block written to 0x100FF000 (4 KB).
//
// ****************************************************************************

#ifndef _ARCADE_PATCHES_H
#define _ARCADE_PATCHES_H

#include "../include.h"

#ifdef __cplusplus
extern "C" {
#endif

// === Patch slot sizes ===
#define ARCADE_ILI9341_INIT_SLOT  114u
#define ARCADE_ILI9341_SIG_LEN    7u
#define ARCADE_ENC16_LOOP_LEN     30u
#define ARCADE_ENC16_SIG_LEN      8u

// LUT and CF2 addresses come from config.h (= switchable between 1 MB
// and 2 MB layouts via LAUNCHER_2MB flag). The values here keep names
// expected by the rest of the launcher code.
#define ARCADE_LUT_ADDR           LAUNCHER_LUT_ADDR
#define ARCADE_LUT_LEN            64u

#define ARCADE_CF2_ADDR           LAUNCHER_CF2_ADDR
#define ARCADE_CF2_LEN            4096u

// === Signatures ===
extern const u8 ARCADE_ILI9341_SIG[ARCADE_ILI9341_SIG_LEN];
extern const u8 ARCADE_ENC16_SIG[ARCADE_ENC16_SIG_LEN];

// === Replacement payloads ===
extern const u8 ARCADE_ST7789_INIT[];     // up to 114 B (zero-padded)
extern const u8 ARCADE_PALETTE_PATCH[ARCADE_ENC16_LOOP_LEN];
extern const u8 ARCADE_PALETTE_LUT[ARCADE_LUT_LEN];

// === CF2 sector ===
// Generates a 4 KB CF2 block with PicoPad-specific config. Returns byte count
// written to dst (= 4096).
u32 ArcadeBuildCf2(u8* dst);

// === Game metadata ===
// Read the game name from the flashed Arcade binary by scanning the game
// region for a JSON metadata block (= '"name":"..."'). Returns True if
// found and copied to out (NUL-terminated, max max_len incl. NUL).
Bool ArcadeReadName(char* out, u32 max_len);

#ifdef __cplusplus
}
#endif

#endif // _ARCADE_PATCHES_H
