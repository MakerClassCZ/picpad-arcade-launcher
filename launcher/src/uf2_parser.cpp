// ****************************************************************************
//
//                  PicoPad Arcade Launcher — UF2 parser
//
// ****************************************************************************

#include "../include.h"
#include "uf2_parser.h"

static u32 ReadLE32(const u8* p)
{
    return ((u32)p[0]) | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

Uf2TargetKind Uf2ClassifyAddr(u32 addr)
{
    if (addr >= 0x10000000u && addr < 0x11000000u) return UF2_TARGET_FLASH;
    if (addr >= 0x20000000u && addr < 0x21000000u) return UF2_TARGET_RAM;
    return UF2_TARGET_UNKNOWN;
}

Bool Uf2ReadInfo(const char* path, Uf2Info* info)
{
    sFile file;
    FileInit(&file);
    if (!FileOpen(&file, path)) return False;

    u8 block[UF2_BLOCK_SIZE];

    // Try first 8 blocks until we find a valid one (some UF2 files include
    // an "abs-block" prelude on RP2350 — skip non-conforming blocks).
    Bool found = False;
    for (int attempt = 0; attempt < 8; attempt++)
    {
        u32 n = FileRead(&file, block, UF2_BLOCK_SIZE);
        if (n != UF2_BLOCK_SIZE) break;

        // Check magic
        if (ReadLE32(block + 0) != UF2_MAGIC_START_0) continue;
        if (ReadLE32(block + 4) != UF2_MAGIC_START_1) continue;
        if (ReadLE32(block + 508) != UF2_MAGIC_END)   continue;

        u32 flags        = ReadLE32(block + 8);
        u32 target_addr  = ReadLE32(block + 12);
        u32 payload_size = ReadLE32(block + 16);
        // u32 block_no  = ReadLE32(block + 20);
        u32 num_blocks   = ReadLE32(block + 24);
        u32 family_id    = (flags & UF2_FLAG_FAMILY_ID) ? ReadLE32(block + 28) : 0;

        info->target_addr      = target_addr;
        info->family_id        = family_id;
        info->num_blocks       = num_blocks;
        info->total_size_bytes = num_blocks * payload_size;
        info->kind             = Uf2ClassifyAddr(target_addr);

        found = True;
        break;
    }

    FileClose(&file);
    return found;
}
