// ****************************************************************************
//
//                  PicoPad Arcade Launcher — UF2 parser
//
// ****************************************************************************

#ifndef _UF2_PARSER_H
#define _UF2_PARSER_H

#include "../include.h"

#ifdef __cplusplus
extern "C" {
#endif

// UF2 block constants
#define UF2_MAGIC_START_0   0x0A324655u  // "UF2\n"
#define UF2_MAGIC_START_1   0x9E5D5157u
#define UF2_MAGIC_END       0x0AB16F30u
#define UF2_FLAG_FAMILY_ID  0x00002000u
#define UF2_BLOCK_SIZE      512
#define UF2_PAYLOAD_SIZE    256

// Standard family IDs
#define FAMILY_RP2040       0xE48BFF56u
#define FAMILY_RP2350_ARM_S 0xE48BFF59u
#define FAMILY_RP2350_RISCV 0xE48BFF5Au

// Memory range types for target_addr
typedef enum {
    UF2_TARGET_FLASH,    // 0x10000000..0x10FFFFFF
    UF2_TARGET_RAM,      // 0x20000000..0x20FFFFFF
    UF2_TARGET_UNKNOWN,
} Uf2TargetKind;

// UF2 file metadata read from the first valid block
typedef struct {
    u32 target_addr;       // base of first block
    u32 family_id;
    u32 num_blocks;        // declared total
    u32 total_size_bytes;  // num_blocks * 256
    Uf2TargetKind kind;
} Uf2Info;

// Parse the first valid UF2 block from the file.
// Returns True if a valid block was found and info is filled.
// Leaves file closed at exit.
Bool Uf2ReadInfo(const char* path, Uf2Info* info);

// Classify target_addr into kind
Uf2TargetKind Uf2ClassifyAddr(u32 addr);

#ifdef __cplusplus
}
#endif

#endif // _UF2_PARSER_H
