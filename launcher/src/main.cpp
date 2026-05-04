// ****************************************************************************
//
//                    PicoPad Arcade Launcher — main entry
//
//   M2: Initializes display, hands off to menu state machine.
//   Menu provides: PLAY, LOAD from SD, USB CARD READER, SETTINGS,
//                  REBOOT TO BOOTSEL.
//
// ****************************************************************************

#include "../include.h"
#include "menu.h"

int main()
{
    // Switch to 8x8 system font. PicoLibSDK default is FontBold8x16, but we
    // sized the menu UI for 8x8 (= MENU_LINE_H = 20, fits 8 px lines with
    // padding). Explicitly selecting the 8x8 font also lets --gc-sections
    // strip the unused FontBold8x16 (= ~4 KB rodata saving).
    SelFont8x8();

    MenuRun();
    return 0;
}
