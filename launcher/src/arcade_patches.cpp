// ****************************************************************************
//
//      PicoPad Arcade Launcher — patch payloads
//
// ****************************************************************************

#include "../include.h"
#include "arcade_patches.h"

// ---------------------------------------------------------------------------
// Signatures (= unique byte sequences that identify patch sites in Arcade UF2)
// ---------------------------------------------------------------------------

const u8 ARCADE_ILI9341_SIG[ARCADE_ILI9341_SIG_LEN] = {
    0xEF, 0x03, 0x03, 0x80, 0x02, 0xCF, 0x03,
};

const u8 ARCADE_ENC16_SIG[ARCADE_ENC16_SIG_LEN] = {
    0x4B, 0x01, 0x0B, 0x43, 0xCC, 0x10, 0x04, 0x43,
};

// ---------------------------------------------------------------------------
// ST7789 init replacement (23 bytes used, slot is 114 bytes, rest zero-padded)
// ---------------------------------------------------------------------------

const u8 ARCADE_ST7789_INIT[ARCADE_ILI9341_INIT_SLOT] = {
    0x01, 0x80, 150,             // SWRESET + 150ms
    0x11, 0x80, 255,             // SLPOUT + 255ms
    0x3A, 0x81, 0x55, 10,        // COLMOD = 0x55 (16bpp RGB565) + 10ms
    0x36, 0x01, 0x40,            // MADCTL = 0x40 (MV=0)
    0x21, 0x00,                  // INVON (IPS)
    0x13, 0x80, 10,              // NORON + 10ms
    0x29, 0x80, 100,             // DISPON + 100ms
    0x00, 0x00,
    // Remaining bytes (114 - 25 = 89) are 0x00 (zero-init by C)
};

// ---------------------------------------------------------------------------
// Palette LUT copy stub (30 bytes ARM Thumb-1, embedded LUT address)
//
// Layout:
//   +0x00: 03 4B          ldr r3, [pc, #0x0C]   -> r3 = LUT_ADDR
//   +0x02: 10 24          movs r4, #16          -> counter
//   +0x04: 01 CB          ldm r3!, {r0}         -> LOOP: r0 = *r3++
//   +0x06: 01 C2          stm r2!, {r0}         -> *r2++ = r0
//   +0x08: 01 3C          subs r4, #1
//   +0x0A: FB D1          bne -6                -> back to LOOP
//   +0x0C: 07 E0          b +0x0E               -> skip literal
//   +0x0E: 00 BF          nop (align)
//   +0x10: 00 00 08 10    .word 0x10080000      -> LUT_ADDR (little-endian)
//   +0x14..0x1D: 5x nop padding
// ---------------------------------------------------------------------------

// LUT address embedded at offset 0x10 must match LAUNCHER_LUT_ADDR. Use
// preprocessor stringization of bytes — generated for either 1 MB or 2 MB
// layout based on config.h.
#define LUT_BYTE0  ((u8)((LAUNCHER_LUT_ADDR      ) & 0xFF))
#define LUT_BYTE1  ((u8)((LAUNCHER_LUT_ADDR >>  8) & 0xFF))
#define LUT_BYTE2  ((u8)((LAUNCHER_LUT_ADDR >> 16) & 0xFF))
#define LUT_BYTE3  ((u8)((LAUNCHER_LUT_ADDR >> 24) & 0xFF))

const u8 ARCADE_PALETTE_PATCH[ARCADE_ENC16_LOOP_LEN] = {
    0x03, 0x4B,                              // ldr r3, [pc, #0x0C]
    0x10, 0x24,                              // movs r4, #16
    0x01, 0xCB,                              // ldm r3!, {r0}
    0x01, 0xC2,                              // stm r2!, {r0}
    0x01, 0x3C,                              // subs r4, #1
    0xFB, 0xD1,                              // bne -6
    0x07, 0xE0,                              // b +0x0E
    0x00, 0xBF,                              // nop
    LUT_BYTE0, LUT_BYTE1, LUT_BYTE2, LUT_BYTE3,  // .word LAUNCHER_LUT_ADDR (LE)
    0x00, 0xBF, 0x00, 0xBF, 0x00, 0xBF,      // 3 × nop
    0x00, 0xBF, 0x00, 0xBF,                  // 2 × nop (total 5 nop padding)
};

// ---------------------------------------------------------------------------
// MakeCode Arcade default 16-color palette in RGB565 doubled format (4B each)
// = identical to PCC patcher's ARCADE_PALETTE → enc16 → << 16 | itself
// ---------------------------------------------------------------------------

const u8 ARCADE_PALETTE_LUT[ARCADE_LUT_LEN] = {
    0x00, 0x00, 0x00, 0x00,   // black
    0xff, 0xff, 0xff, 0xff,   // white
    0xf9, 0x04, 0xf9, 0x04,   // red
    0xfc, 0x98, 0xfc, 0x98,   // pink
    0xfc, 0x06, 0xfc, 0x06,   // orange
    0xff, 0xa1, 0xff, 0xa1,   // yellow
    0x24, 0xf4, 0x24, 0xf4,   // teal
    0x7e, 0xea, 0x7e, 0xea,   // green
    0x01, 0xf5, 0x01, 0xf5,   // blue
    0x87, 0x9f, 0x87, 0x9f,   // light blue
    0x89, 0x78, 0x89, 0x78,   // purple
    0xa4, 0x13, 0xa4, 0x13,   // light gray
    0x5a, 0x0d, 0x5a, 0x0d,   // dark gray
    0xe6, 0x78, 0xe6, 0x78,   // light pink
    0x92, 0x27, 0x92, 0x27,   // brown
    0x00, 0x00, 0x00, 0x00,   // black (alpha)
};

// ---------------------------------------------------------------------------
// CF2 sector — 4 KB with PicoPad-specific config
// ---------------------------------------------------------------------------

static const u8 CF2_MAGIC[8] = {
    0xF1, 0x10, 0x9E, 0x1E, 0x79, 0x7A, 0x22, 0x20,
};

static const struct { u32 key; u32 val; } CF2_ENTRIES[] = {
    {0x04, 7},        {0x05, 6},
    {0x20, 18},       {0x22, 19},     {0x23, 21},     {0x24, 17},
    {0x25, 320},      {0x26, 240},
    {0x27, 0x40},     {0x28, 0xFFFFFF}, {0x29, 0x28},
    {0x2B, 20},       {0x2C, 16},
    {0x2F, 3},        {0x30, 2},      {0x31, 4},      {0x32, 5},      {0x33, 9},
    {0x3C, 9},        {0x41, 15},
    {0x47, 0},        {0x48, 1},      {0x49, 2},
    {0x4E, 9341},
};
#define CF2_ENTRY_COUNT (sizeof(CF2_ENTRIES) / sizeof(CF2_ENTRIES[0]))

static void Pack32LE(u8* dst, u32 v)
{
    dst[0] = (u8)v;
    dst[1] = (u8)(v >> 8);
    dst[2] = (u8)(v >> 16);
    dst[3] = (u8)(v >> 24);
}

// ---------------------------------------------------------------------------
// Game name lookup — scan game region for the MakeCode Arcade JSON metadata
// block (e.g. {"compression":"LZMA",...,"name":"delivery","eURL":...})
// ---------------------------------------------------------------------------

Bool ArcadeReadName(char* out, u32 max_len)
{
    if (max_len < 2) return False;

    static const char NAME_KEY[] = "\"name\":\"";
    const u32 KEY_LEN = sizeof(NAME_KEY) - 1;
    const u8* base = (const u8*)0x10000100u;
    const u32 size = LAUNCHER_GAME_HI - 0x10000100u;

    for (u32 off = 0; off + KEY_LEN <= size; off++) {
        if (memcmp(base + off, NAME_KEY, KEY_LEN) != 0) continue;
        const char* s = (const char*)(base + off + KEY_LEN);
        u32 i = 0;
        while (i < max_len - 1 && s[i] != '"' && s[i] != 0) {
            out[i] = s[i];
            i++;
        }
        out[i] = 0;
        return i > 0;
    }
    return False;
}

u32 ArcadeBuildCf2(u8* dst)
{
    // Initialize whole sector to 0x00 — matches web patcher exactly.
    // 0xFF padding would cause firmware to read garbage entries past
    // the (0,0) terminator and apply wrong config.
    memset(dst, 0x00, ARCADE_CF2_LEN);

    // Magic
    memcpy(dst, CF2_MAGIC, 8);

    // Header: count + format version
    Pack32LE(dst + 8,  CF2_ENTRY_COUNT);
    Pack32LE(dst + 12, 0x1FA);

    // Entries
    int p = 16;
    for (u32 i = 0; i < CF2_ENTRY_COUNT; i++) {
        Pack32LE(dst + p,     CF2_ENTRIES[i].key);
        Pack32LE(dst + p + 4, CF2_ENTRIES[i].val);
        p += 8;
    }
    // Terminator: (0, 0)
    Pack32LE(dst + p,     0);
    Pack32LE(dst + p + 4, 0);

    return ARCADE_CF2_LEN;
}
