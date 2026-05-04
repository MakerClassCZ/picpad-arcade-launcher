// ****************************************************************************
//
//             PicoPad Arcade Launcher — SD card file picker
//
// ****************************************************************************

#ifndef _FILE_PICKER_H
#define _FILE_PICKER_H

#include "../include.h"

#ifdef __cplusplus
extern "C" {
#endif

// Maximum number of files in directory listing
#define PICKER_MAX_FILES   64

// Maximum file name length (8.3 short name = 12 chars + ext, with safety)
#define PICKER_NAME_LEN    32

// Result of FilePickerRun
typedef enum {
    PICKER_RESULT_CANCEL,    // user pressed B / no files / mount fail
    PICKER_RESULT_SELECTED,  // user picked a file (out_path holds full path)
} PickerResult;

// Show file picker browsing the given directory.
//  dir_path:  directory to list (e.g. "/arcade")
//  pattern:   filename glob (e.g. "*.UF2"), NULL for all
//  out_path:  buffer to receive full path of selected file
//  out_len:   size of out_path buffer
PickerResult FilePickerRun(const char* dir_path, const char* pattern,
                           char* out_path, int out_len);

#ifdef __cplusplus
}
#endif

#endif // _FILE_PICKER_H
