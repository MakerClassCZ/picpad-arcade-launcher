// ****************************************************************************
//
//                  PicoPad Arcade Launcher — main menu
//
// ****************************************************************************

#ifndef _MENU_H
#define _MENU_H

#include "../include.h"

#ifdef __cplusplus
extern "C" {
#endif

// Menu items
typedef enum {
    MENU_PLAY = 0,
    MENU_LOAD,
#if LAUNCHER_HAS_USB_MSC
    MENU_SD_READER,
#endif
#if LAUNCHER_HAS_DEBUG_SHELL
    MENU_DEBUG,
#endif
    MENU_BOOTSEL,
    MENU_COUNT,
} MenuItem;

// Address of the game vector table — first thing flashed by Arcade UF2
#define GAME_VECTOR_TABLE_ADDR 0x10000100u

// Memory bounds for game vector table validation
#define RAM_BASE     0x20000000u
#define RAM_END      0x20042000u
#define GAME_FLASH_LO  0x10000100u
#define GAME_FLASH_HI  LAUNCHER_GAME_HI    // = 0x100F0000 (1 MB) or 0x101F0000 (2 MB)

// Public entry point — called from main(), never returns
void MenuRun(void);

// Game launch helpers
Bool IsGameValid(void);
void RunGame(void);

// Reboot to true RP2040 BOOTSEL mode (USB MSC drive RPI-RP2)
void RebootToBootsel(void);

#ifdef __cplusplus
}
#endif

#endif // _MENU_H
