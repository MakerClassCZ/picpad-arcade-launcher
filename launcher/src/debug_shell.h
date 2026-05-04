// ****************************************************************************
//
//                  PicoPad Arcade Launcher — debug shell over USB CDC
//
// ****************************************************************************

#ifndef _DEBUG_SHELL_H
#define _DEBUG_SHELL_H

#include "../include.h"

#ifdef __cplusplus
extern "C" {
#endif

// Run interactive debug shell over USB CDC.
// Returns when user types 'exit' or presses B on the device.
void DebugShellRun(void);

#ifdef __cplusplus
}
#endif

#endif // _DEBUG_SHELL_H
