// ****************************************************************************
//
//                  PicoPad Arcade Launcher — flash dialog
//
// ****************************************************************************

#ifndef _FLASH_DIALOG_H
#define _FLASH_DIALOG_H

#include "../include.h"
#include "uf2_parser.h"
#include "flasher.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DLG_CANCEL,
    DLG_PROCEED,   // user confirmed (RAM run or Flash)
} DialogResult;

// Show confirmation dialog with UF2 info + options.
// On RAM-target UF2 the patch checkboxes are hidden (= patches not applicable).
// On Flash-target UF2 the user can adjust patch flags before confirming.
// Returns DLG_PROCEED if user pressed A, DLG_CANCEL otherwise.
DialogResult FlashDialogShow(const char* path, const Uf2Info* info,
                             FlashOptions* opts /* in/out */);

#ifdef __cplusplus
}
#endif

#endif // _FLASH_DIALOG_H
