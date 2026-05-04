// ****************************************************************************
//
//             PicoPad Arcade Launcher — USB MSC SD card reader
//
//   Exposes the SD card as a USB Mass Storage device. Host PC mounts it as
//   a removable drive; user can drag-drop UF2 / config files. Exits on B
//   button or PC eject (= host disables MSC interface).
//
// ****************************************************************************

#ifndef _USB_MSC_SCREEN_H
#define _USB_MSC_SCREEN_H

#include "../include.h"

#ifdef __cplusplus
extern "C" {
#endif

#if LAUNCHER_HAS_USB_MSC
// Enter SD card reader mode. Initializes USB MSC, runs IO loop until exit
// (= B button), then triggers a watchdog reset to ensure clean USB state.
// Does not return on success path.
void UsbMscScreenRun(void);
#endif

#ifdef __cplusplus
}
#endif

#endif // _USB_MSC_SCREEN_H
