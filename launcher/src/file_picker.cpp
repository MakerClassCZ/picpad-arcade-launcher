// ****************************************************************************
//
//             PicoPad Arcade Launcher — SD file picker
//
//   Browses SD card with full sub-folder navigation. Returns full path to
//   the selected file. Folders and UF2 files are both shown — folders first.
//
// ****************************************************************************

#include "../include.h"
#include "file_picker.h"

// Layout
#define PICKER_TITLE_Y      14
#define PICKER_DIVIDER_Y    32
#define PICKER_LIST_Y       46
#define PICKER_LINE_H       12
#define PICKER_FOOTER_Y     220
#define PICKER_VISIBLE      14   // (PICKER_FOOTER_Y - PICKER_LIST_Y) / PICKER_LINE_H
#define PICKER_NAME_X       30
#define MAX_PATH            256

// Entry types in the listing
#define ENT_FILE   0
#define ENT_DIR    1
#define ENT_PARENT 2  // ".." synthetic entry

// Listing entry (~33 B per item, 64 items = ~2 KB)
typedef struct {
    char name[PICKER_NAME_LEN];
    u8   type;   // ENT_*
} PickerEntry;

static PickerEntry g_entries[PICKER_MAX_FILES];
static int  g_entry_count;
static char g_current_path[MAX_PATH];

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static int TextWidth(const char* s)
{
    return (int)strlen(s) * FONTW;
}

static void DrawCenteredText(const char* s, int y, u16 color)
{
    DrawText(s, (WIDTH - TextWidth(s)) / 2, y, color);
}

static void ShowMessage(const char* title, const char* line1, const char* line2)
{
    DrawClear();
    DrawText2(title, (WIDTH - (int)strlen(title) * FONTW * 2) / 2, 30,
              COL_YELLOW);
    if (line1) DrawCenteredText(line1, 90, COL_WHITE);
    if (line2) DrawCenteredText(line2, 110, COL_LTGRAY);
    DrawCenteredText("Press B to return", 200, COL_GRAY);
    DispUpdate();

    while (1)
    {
        u8 k = KeyGet();
        if (k == KEY_B) return;
    }
}

// True if filename matches pattern (only suffix wildcard "*.EXT" supported here).
// PicoLibSDK FindNext accepts patterns directly, but we filter on our side
// because we list folders + files together.
static Bool NameMatchesUF2(const char* name)
{
    int n = (int)strlen(name);
    if (n < 5) return False;  // need at least "X.UF2"
    // Case-insensitive suffix compare against ".UF2"
    return (
        (name[n-4] == '.') &&
        (name[n-3] == 'U' || name[n-3] == 'u') &&
        (name[n-2] == 'F' || name[n-2] == 'f') &&
        (name[n-1] == '2')
    );
}

// ---------------------------------------------------------------------------
// Path manipulation
// ---------------------------------------------------------------------------

// Append "/name" to path, return new length (or -1 on overflow).
static int PathPush(char* path, const char* name, int max_len)
{
    int plen = (int)strlen(path);
    int nlen = (int)strlen(name);
    // Add separator unless path already ends with /
    Bool need_slash = (plen > 0) && (path[plen-1] != '/');
    int total = plen + (need_slash ? 1 : 0) + nlen + 1;
    if (total > max_len) return -1;
    if (need_slash) path[plen++] = '/';
    memcpy(path + plen, name, nlen);
    path[plen + nlen] = 0;
    return plen + nlen;
}

// Strip last component off path. Returns False if already at root.
static Bool PathPop(char* path)
{
    int n = (int)strlen(path);
    if (n <= 1) return False;  // already root "/" or empty
    // Strip trailing slash if any (shouldn't be there normally)
    if (path[n-1] == '/') { path[n-1] = 0; n--; }
    // Find last '/'
    int i = n - 1;
    while (i > 0 && path[i] != '/') i--;
    if (i == 0) {
        path[0] = '/';
        path[1] = 0;
    } else {
        path[i] = 0;
    }
    return True;
}

static Bool PathIsRoot(const char* path)
{
    return path[0] == '/' && path[1] == 0;
}

// ---------------------------------------------------------------------------
// Directory enumeration
// ---------------------------------------------------------------------------

static int LoadDirectory(const char* dir_path)
{
    g_entry_count = 0;

    // Add ".." entry if not at root
    if (!PathIsRoot(dir_path))
    {
        strcpy(g_entries[g_entry_count].name, "..");
        g_entries[g_entry_count].type = ENT_PARENT;
        g_entry_count++;
    }

    sFile find;
    sFileInfo info;

    FileInit(&find);
    if (!FindOpen(&find, dir_path)) return -1;

    int first_dir = g_entry_count;

    // First pass: collect directories
    while (FindNext(&find, &info, ATTR_DIR_MASK, "*"))
    {
        if (g_entry_count >= PICKER_MAX_FILES) break;
        if (info.namelen == 0) continue;
        if (info.name[0] == '.') continue;  // skip hidden / "." / ".."

        if (info.attr & ATTR_DIR)
        {
            int n = info.namelen;
            if (n >= PICKER_NAME_LEN) n = PICKER_NAME_LEN - 1;
            memcpy(g_entries[g_entry_count].name, info.name, n);
            g_entries[g_entry_count].name[n] = 0;
            g_entries[g_entry_count].type = ENT_DIR;
            g_entry_count++;
        }
    }

    FindClose(&find);

    int first_file = g_entry_count;

    // Second pass: collect UF2 files
    FileInit(&find);
    if (!FindOpen(&find, dir_path)) return g_entry_count;

    while (FindNext(&find, &info, ATTR_ARCH, "*"))
    {
        if (g_entry_count >= PICKER_MAX_FILES) break;
        if (info.namelen == 0) continue;
        if (info.attr & ATTR_DIR) continue;
        if (info.name[0] == '.') continue;

        if (NameMatchesUF2(info.name))
        {
            int n = info.namelen;
            if (n >= PICKER_NAME_LEN) n = PICKER_NAME_LEN - 1;
            memcpy(g_entries[g_entry_count].name, info.name, n);
            g_entries[g_entry_count].name[n] = 0;
            g_entries[g_entry_count].type = ENT_FILE;
            g_entry_count++;
        }
    }

    FindClose(&find);

    // Sort dirs and files separately, alphabetically (case-insensitive).
    // FAT FindNext returns entries in directory creation order, which is
    // unfriendly for browsing.
    auto cmp = [](const char* a, const char* b) -> int {
        while (*a && *b) {
            char ca = (*a >= 'A' && *a <= 'Z') ? (char)(*a + 32) : *a;
            char cb = (*b >= 'A' && *b <= 'Z') ? (char)(*b + 32) : *b;
            if (ca != cb) return (u8)ca - (u8)cb;
            a++; b++;
        }
        return (u8)*a - (u8)*b;
    };
    auto sort_range = [&](int start, int end) {
        for (int i = start; i < end - 1; i++) {
            for (int j = start; j < end - 1 - (i - start); j++) {
                if (cmp(g_entries[j].name, g_entries[j+1].name) > 0) {
                    PickerEntry tmp = g_entries[j];
                    g_entries[j] = g_entries[j+1];
                    g_entries[j+1] = tmp;
                }
            }
        }
    };
    sort_range(first_dir, first_file);    // directories
    sort_range(first_file, g_entry_count); // files

    return g_entry_count;
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

static void RenderList(int selected, int scroll_top)
{
    DrawClear();

    // Title — current path (truncated if too long)
    char title[64];
    int max_chars = sizeof(title) - 1;
    int plen = (int)strlen(g_current_path);
    if (plen <= max_chars) {
        strcpy(title, g_current_path);
    } else {
        // Show last max_chars
        strcpy(title, "...");
        strcat(title, g_current_path + plen - (max_chars - 3));
    }
    DrawText(title, 10, PICKER_TITLE_Y, COL_YELLOW);

    DrawRect(10, PICKER_DIVIDER_Y, WIDTH - 20, 1, COL_GRAY);

    // List entries
    for (int i = 0; i < PICKER_VISIBLE; i++)
    {
        int idx = scroll_top + i;
        if (idx >= g_entry_count) break;

        int y = PICKER_LIST_Y + i * PICKER_LINE_H;
        const PickerEntry* e = &g_entries[idx];

        u16 color, type_color;
        const char* arrow;
        const char* prefix;

        if (idx == selected)
        {
            arrow = ">";
            DrawRect(PICKER_NAME_X - 14, y - 1, WIDTH - 2 * (PICKER_NAME_X - 14),
                     FONTH + 2, RGBTO16(40, 40, 0));
            color = COL_YELLOW;
        }
        else
        {
            arrow = " ";
            color = COL_WHITE;
        }

        switch (e->type)
        {
        case ENT_DIR:
            prefix = "[D]";
            type_color = COL_AZURE;
            break;
        case ENT_PARENT:
            prefix = "[<]";
            type_color = COL_GREEN;
            break;
        default:
            prefix = "   ";
            type_color = color;
            break;
        }

        DrawText(arrow,  12, y, color);
        DrawText(prefix, 22, y, (idx == selected) ? COL_YELLOW : type_color);
        DrawText(e->name, PICKER_NAME_X + 22, y, color);
    }

    // Footer divider
    DrawRect(10, PICKER_FOOTER_Y - 8, WIDTH - 20, 1, COL_GRAY);

    // Footer status
    char footer[40];
    if (g_entry_count == 0) {
        strcpy(footer, "Empty folder");
    } else {
        char numbuf[16];
        DecNum(numbuf, selected + 1, 0);
        strcpy(footer, numbuf);
        strcat(footer, " / ");
        DecNum(numbuf, g_entry_count, 0);
        strcat(footer, numbuf);
    }
    DrawText(footer, 10, PICKER_FOOTER_Y, COL_LTGRAY);
    DrawText("A=Open  B=Up/Cancel",
             WIDTH - 19*FONTW, PICKER_FOOTER_Y, COL_LTGRAY);

    DispUpdate();
}

// ---------------------------------------------------------------------------
// Main entry
// ---------------------------------------------------------------------------

PickerResult FilePickerRun(const char* dir_path, const char* pattern,
                           char* out_path, int out_len)
{
    (void)pattern;  // suffix filter is hardcoded to UF2 in NameMatchesUF2

    // Mount SD
    if (!DiskAutoMount())
    {
        ShowMessage("LOAD from SD",
                    "SD card not found",
                    "Insert SD card and try again");
        return PICKER_RESULT_CANCEL;
    }

    // Initialize current path from caller
    strcpy(g_current_path, dir_path);
    if (g_current_path[0] == 0) strcpy(g_current_path, "/");

    int selected = 0;
    int scroll_top = 0;
    Bool need_reload = True;
    Bool need_redraw = True;

    while (1)
    {
        if (need_reload)
        {
            int n = LoadDirectory(g_current_path);
            if (n < 0)
            {
                char msg[80];
                strcpy(msg, "Cannot open ");
                strcat(msg, g_current_path);
                ShowMessage("LOAD from SD", msg,
                            "Going back to root");
                strcpy(g_current_path, "/");
                continue;
            }
            selected = 0;
            scroll_top = 0;
            need_reload = False;
            need_redraw = True;
        }

        // Adjust scroll
        if (selected < scroll_top) scroll_top = selected;
        if (selected >= scroll_top + PICKER_VISIBLE)
            scroll_top = selected - PICKER_VISIBLE + 1;
        if (scroll_top < 0) scroll_top = 0;

        if (need_redraw)
        {
            RenderList(selected, scroll_top);
            need_redraw = False;
        }

        u8 k = KeyGet();
        if (k == NOKEY)
        {
            WaitMs(20);
            continue;
        }

        switch (k)
        {
        case KEY_UP:
            if (selected > 0) { selected--; need_redraw = True; }
            break;
        case KEY_DOWN:
            if (selected < g_entry_count - 1) { selected++; need_redraw = True; }
            break;
        case KEY_LEFT:
            selected -= PICKER_VISIBLE;
            if (selected < 0) selected = 0;
            need_redraw = True;
            break;
        case KEY_RIGHT:
            selected += PICKER_VISIBLE;
            if (selected >= g_entry_count) selected = g_entry_count - 1;
            need_redraw = True;
            break;

        case KEY_A:
            if (g_entry_count == 0) break;
            switch (g_entries[selected].type)
            {
            case ENT_PARENT:
                PathPop(g_current_path);
                need_reload = True;
                break;
            case ENT_DIR:
                if (PathPush(g_current_path, g_entries[selected].name,
                             MAX_PATH) < 0) {
                    ShowMessage("Path too long", "", "");
                } else {
                    need_reload = True;
                }
                break;
            case ENT_FILE:
                {
                    // Compose full path
                    int total = (int)strlen(g_current_path) + 1
                              + (int)strlen(g_entries[selected].name) + 1;
                    if (total > out_len) {
                        ShowMessage("Path too long", "", "");
                        break;
                    }
                    strcpy(out_path, g_current_path);
                    if (g_current_path[strlen(g_current_path)-1] != '/')
                        strcat(out_path, "/");
                    strcat(out_path, g_entries[selected].name);
                    return PICKER_RESULT_SELECTED;
                }
            }
            break;

        case KEY_B:
            // At root → cancel; otherwise go up one level
            if (PathIsRoot(g_current_path)) {
                return PICKER_RESULT_CANCEL;
            }
            PathPop(g_current_path);
            need_reload = True;
            break;
        }
    }
}
