// ****************************************************************************
//
//                  PicoPad Arcade Launcher — flasher
//
//   Flow:
//     Phase 1: erase entire game region (0x10000000..0x100EFFFF)
//     Phase 2: stream UF2 payload to flash in one big batch via FrameBuf
//     Phase 3: post-flash patches (scan whole region for ST7789 + ENC16 sigs)
//     Phase 4: write LUT, CF2, and re-write boot2
//
// ****************************************************************************

#include "../include.h"
#include "flasher.h"
#include "uf2_parser.h"
#include "arcade_patches.h"
#include "boot2_embedded.h"

extern "C" void GoToAppRam(void);
extern "C" void RuntimeTerm(void);
extern "C" void KeyWaitNoPressed(void);
extern FRAMETYPE ALIGNED FrameBuf[FRAMESIZE];

#define STAGING_BYTES_MAX  (FRAMESIZE * sizeof(FRAMETYPE))
#define SECTOR_SIZE  4096u

// Sector staging buffer — dedicated .bss, can't reuse FrameBuf because the
// flash-progress UI overwrites it.
static u8 g_sector_buf[SECTOR_SIZE];

// ---------------------------------------------------------------------------
// Protected regions (= our boot2 + launcher + state + CF2)
// ---------------------------------------------------------------------------

typedef struct { u32 start, end; } Region;

static const Region PROTECTED[] = {
    {0x10000000u, 0x10000100u},        // patched boot2
    {LAUNCHER_FLASH_BASE, LAUNCHER_FLASH_END},  // launcher + CF2 area
};
#define PROTECTED_COUNT (sizeof(PROTECTED) / sizeof(PROTECTED[0]))

Bool FlasherIsProtected(u32 addr, u32 size)
{
    u32 end = addr + size;
    for (u32 i = 0; i < PROTECTED_COUNT; i++) {
        if (end > PROTECTED[i].start && addr < PROTECTED[i].end) return True;
    }
    return False;
}

// ---------------------------------------------------------------------------
// RAM bounds
// ---------------------------------------------------------------------------

#define RAM_BASE  0x20000000u
#define RAM_END   0x20040000u

static Bool IsRamRangeOk(u32 addr, u32 size)
{
    return (addr >= RAM_BASE) && ((u64)addr + size <= RAM_END);
}

// ---------------------------------------------------------------------------
// Progress bar
// ---------------------------------------------------------------------------

static void DrawProgress(const char* title, u32 done, u32 total)
{
    DrawClear();
    int title_x = (WIDTH - (int)strlen(title) * FONTW * 2) / 2;
    DrawText2(title, title_x, 30, COL_YELLOW);

    int bar_x = 30, bar_y = 110, bar_w = WIDTH - 60, bar_h = 18;
    DrawRect(bar_x, bar_y, bar_w, bar_h, COL_GRAY);
    if (total > 0) {
        int fill = (int)((u64)bar_w * done / total);
        if (fill > bar_w) fill = bar_w;
        if (fill > 1) DrawRect(bar_x + 1, bar_y + 1, fill - 1, bar_h - 2, COL_GREEN);
    }

    char pct_buf[8];
    u32 pct = (total > 0) ? (u32)((u64)done * 100 / total) : 0;
    DecNum(pct_buf, pct, 0);
    strcat(pct_buf, "%");
    int pct_x = (WIDTH - (int)strlen(pct_buf) * FONTW) / 2;
    DrawText(pct_buf, pct_x, 140, COL_WHITE);

    DispUpdate();
}

// ---------------------------------------------------------------------------
// LE helper
// ---------------------------------------------------------------------------

static u32 ReadLE32(const u8* p)
{
    return ((u32)p[0]) | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

// ---------------------------------------------------------------------------
// RAM mode
// ---------------------------------------------------------------------------

FlashResult FlasherRunRam(const char* path, const Uf2Info* info)
{
    if (!IsRamRangeOk(info->target_addr, info->total_size_bytes))
        return FLASH_ERR_TOO_BIG;
    if (info->total_size_bytes > STAGING_BYTES_MAX)
        return FLASH_ERR_TOO_BIG;
    if (info->family_id != 0 && info->family_id != FAMILY_RP2040)
        return FLASH_ERR_INVALID;

    DrawClear();
    DrawText2("Loading to RAM", 50, 60, COL_YELLOW);
    DrawText("Reading UF2 from SD...", 60, 110, COL_LTGRAY);
    DispUpdate();

    sFile file;
    FileInit(&file);
    if (!FileOpen(&file, path)) return FLASH_ERR_OPEN;

    u8* staging = (u8*)FrameBuf;
    u32 base = info->target_addr;

    u8 block[UF2_BLOCK_SIZE];
    while (1) {
        u32 n = FileRead(&file, block, UF2_BLOCK_SIZE);
        if (n == 0) break;
        if (n != UF2_BLOCK_SIZE) { FileClose(&file); return FLASH_ERR_INVALID; }
        if (ReadLE32(block + 0) != UF2_MAGIC_START_0) continue;
        if (ReadLE32(block + 4) != UF2_MAGIC_START_1) continue;

        u32 target_addr  = ReadLE32(block + 12);
        u32 payload_size = ReadLE32(block + 16);

        if (payload_size > UF2_PAYLOAD_SIZE) { FileClose(&file); return FLASH_ERR_INVALID; }
        if (Uf2ClassifyAddr(target_addr) != UF2_TARGET_RAM) { FileClose(&file); return FLASH_ERR_INVALID; }
        if (!IsRamRangeOk(target_addr, payload_size)) { FileClose(&file); return FLASH_ERR_TOO_BIG; }
        if (target_addr < base) { FileClose(&file); return FLASH_ERR_INVALID; }

        u32 offset = target_addr - base;
        if (offset + payload_size > STAGING_BYTES_MAX) { FileClose(&file); return FLASH_ERR_TOO_BIG; }

        memcpy(staging + offset, &block[32], payload_size);

        if (KeyGet() == KEY_B) { FileClose(&file); return FLASH_ERR_INTERRUPTED; }
    }
    FileClose(&file);

    KeyWaitNoPressed();
    RuntimeTerm();

    __asm volatile ("cpsid i" ::: "memory");

    {
        volatile u8* d = (volatile u8*)base;
        const u8* s = staging;
        u32 n = info->total_size_bytes;
        for (u32 i = 0; i < n; i++) d[i] = s[i];
    }
    GoToAppRam();
    while (1) {}
}

// ---------------------------------------------------------------------------
// Flash mode helpers
// ---------------------------------------------------------------------------

// Scan whole game region for `sig`; if found, splice `payload` at that address
// (= read containing sector, splice in RAM, erase, re-write). Different
// MakeCode runtime builds place the patch sites at different flash offsets,
// so a hardcoded address is unreliable.
static Bool FlashPatchScan(const u8* sig, u32 sig_len,
                            const u8* payload, u32 slot_len)
{
    const u8* fbase = (const u8*)0x10000100u;
    u32 fsize = LAUNCHER_GAME_HI - 0x10000100u;
    for (u32 off = 0; off + sig_len <= fsize; off += 2) {
        if (memcmp(fbase + off, sig, sig_len) != 0) continue;
        u32 found = 0x10000100u + off;
        u32 sec = found & ~(SECTOR_SIZE - 1);
        u32 sec_off = found - sec;
        if (sec_off + slot_len > SECTOR_SIZE) return False;  // slot would cross
        memcpy(g_sector_buf, (const void*)sec, SECTOR_SIZE);
        memcpy(g_sector_buf + sec_off, payload, slot_len);
        FlashErase(sec - 0x10000000u, SECTOR_SIZE);
        FlashProgram(sec - 0x10000000u, g_sector_buf, SECTOR_SIZE);
        return True;
    }
    return False;
}

// Build a 4 KB sector with `data` at offset 0 and `fill` padding, then flash it.
static void FlashSectorAt(u32 sector_addr, const u8* data, u32 len, u8 fill)
{
    memset(g_sector_buf, fill, SECTOR_SIZE);
    memcpy(g_sector_buf, data, len);
    FlashErase(sector_addr - 0x10000000u, SECTOR_SIZE);
    FlashProgram(sector_addr - 0x10000000u, g_sector_buf, SECTOR_SIZE);
}

// Re-write sector 0 with our patched boot2 (preserves the rest of the sector
// from the just-flashed game vector table).
static void RewriteBoot2Sector(void)
{
    memcpy(g_sector_buf, (const void*)0x10000000u, SECTOR_SIZE);
    memcpy(g_sector_buf, boot2_embedded, sizeof(boot2_embedded));
    FlashErase(0, SECTOR_SIZE);
    FlashProgram(0, g_sector_buf, SECTOR_SIZE);
}

// ---------------------------------------------------------------------------
// Flash mode — main flow
// ---------------------------------------------------------------------------

FlashResult FlasherWriteFlash(const char* path, const Uf2Info* info,
                              const FlashOptions* opts)
{
    if (info->kind != UF2_TARGET_FLASH) return FLASH_ERR_INVALID;
    // Reject UF2s for other MCUs (= protect against drag-drop of wrong file).
    if (info->family_id != 0 && info->family_id != FAMILY_RP2040) {
        return FLASH_ERR_INVALID;
    }

    sFile file;
    FileInit(&file);
    if (!FileOpen(&file, path)) return FLASH_ERR_OPEN;

    // Phase 1: tail-erase only the region we're about to write, rounded up
    // to 64 KB chunks. PicoLibSDK FlashErase picks the 64KB-block flash
    // command for that size — much faster than per-sector erases.
    // Note: LUT (0x10080000) and CF2 (0x100FF000) sectors are erased
    // separately during their own write phase.
    {
        const u32 ERASE_CHUNK = 64u * 1024u;
        u32 erase_size = (info->total_size_bytes + ERASE_CHUNK - 1)
                          & ~(ERASE_CHUNK - 1);
        const u32 max_size = LAUNCHER_GAME_HI - 0x10000000u;
        if (erase_size > max_size) erase_size = max_size;
        for (u32 off = 0; off < erase_size; off += ERASE_CHUNK) {
            FlashErase(off, ERASE_CHUNK);
            DrawProgress("Erasing", off + ERASE_CHUNK, erase_size);
        }
    }

    // Phase 2: stream payload to flash in 16 KB batches via FrameBuf.
    // Batch size balances progress granularity (~18 updates for ~290 KB)
    // with FlashProgram throughput. DrawProgress fills FrameBuf with pixels;
    // we then overwrite that RAM with batch payload — display has already
    // DMA'd the previous progress frame, so no visible glitch.
    #define BATCH_BYTES (16u * 1024u)
    u8* batch_buf = (u8*)FrameBuf;
    u32 file_pos = 32;  // payload start of block 0
    u32 cur_target = 0x10000000u;
    u32 total_payload = info->num_blocks * UF2_PAYLOAD_SIZE;
    u32 end_target = 0x10000000u + total_payload;
    u32 done_bytes = 0;

    while (cur_target < end_target) {
        DrawProgress("Flashing", done_bytes, total_payload);

        u32 batch_off = 0;
        u32 batch_start = cur_target;
        while (batch_off < BATCH_BYTES && cur_target < end_target) {
            Bool skip = opts->preserve_launcher
                && (cur_target == 0x10000000u
                    || (cur_target >= LAUNCHER_FLASH_BASE
                        && cur_target < LAUNCHER_FLASH_END));
            if (skip) {
                memset(batch_buf + batch_off, 0xFF, UF2_PAYLOAD_SIZE);
            } else {
                FileSeek(&file, file_pos);
                if (FileRead(&file, batch_buf + batch_off, UF2_PAYLOAD_SIZE)
                    != UF2_PAYLOAD_SIZE) {
                    FileClose(&file);
                    return FLASH_ERR_INVALID;
                }
            }
            batch_off += UF2_PAYLOAD_SIZE;
            cur_target += UF2_PAYLOAD_SIZE;
            file_pos += UF2_BLOCK_SIZE;
        }

        FlashProgram(batch_start - 0x10000000u, batch_buf, batch_off);
        done_bytes += batch_off;
    }
    FileClose(&file);
    DrawProgress("Flashing", total_payload, total_payload);

    // Phase 3: scan-based patches + boot2 rewrite + LUT + CF2.
    Bool st7789_ok = False, palette_ok = False;
    if (opts->apply_st7789_init) {
        st7789_ok = FlashPatchScan(ARCADE_ILI9341_SIG, ARCADE_ILI9341_SIG_LEN,
                                    ARCADE_ST7789_INIT, ARCADE_ILI9341_INIT_SLOT);
    }
    if (opts->apply_palette_lut) {
        palette_ok = FlashPatchScan(ARCADE_ENC16_SIG, ARCADE_ENC16_SIG_LEN,
                                     ARCADE_PALETTE_PATCH, ARCADE_ENC16_LOOP_LEN);
    }
    if (opts->preserve_launcher) RewriteBoot2Sector();

    if (opts->apply_palette_lut) {
        FlashSectorAt(ARCADE_LUT_ADDR, ARCADE_PALETTE_LUT, ARCADE_LUT_LEN, 0xFF);
    }
    if (opts->apply_cf2) {
        ArcadeBuildCf2(g_sector_buf);
        FlashErase(ARCADE_CF2_ADDR - 0x10000000u, SECTOR_SIZE);
        FlashProgram(ARCADE_CF2_ADDR - 0x10000000u, g_sector_buf, SECTOR_SIZE);
    }

    // Done screen — auto-reset after a short window so the player can read
    // the patch checklist. Holding B during the window pauses the screen.
    DrawClear();
    DrawText2("Flash done", 80, 14, COL_GREEN);
    int y = 60;
    auto line = [&](const char* label, Bool requested, Bool ok) {
        u16 col;
        const char* prefix;
        if (!requested)   { col = COL_GRAY;  prefix = "[ -] "; }
        else if (ok)      { col = COL_GREEN; prefix = "[OK] "; }
        else              { col = COL_RED;   prefix = "[--] "; }
        char buf[64];
        strcpy(buf, prefix);
        strcat(buf, label);
        DrawText(buf, 30, y, col);
        y += 14;
    };
    line("ST7789 init",     opts->apply_st7789_init, st7789_ok);
    line("Palette stub",    opts->apply_palette_lut, palette_ok);
    line("LUT @ 0x10080000", opts->apply_palette_lut, True);
    line("CF2 @ 0x100FF000", opts->apply_cf2,         True);
    if (opts->apply_st7789_init && !st7789_ok) {
        DrawText("WARNING: ST7789 sig not matched —", 20, y + 8, COL_YELLOW);
        DrawText("display may stay black.", 20, y + 22, COL_YELLOW);
    }
    DrawText("Restarting in 2s    Hold B to stay",
             (WIDTH - 35*FONTW)/2, 215, COL_LTGRAY);
    DispUpdate();

    // Wait up to 2 s, then auto-reset. If user holds B, stay until release.
    for (int t = 0; t < 100; t++) {
        if (KeyPressed(KEY_B)) {
            while (KeyPressed(KEY_B)) WaitMs(50);
            while (!KeyPressed(KEY_A) && !KeyPressed(KEY_B)) WaitMs(50);
            break;
        }
        WaitMs(20);
    }

    KeyWaitNoPressed();
    RuntimeTerm();
    WatchdogSetup(0, False);
    while (1) {}  // unreachable
}
