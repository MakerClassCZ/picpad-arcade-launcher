// ****************************************************************************
//
//                  PicoPad Arcade Launcher — flash dialog
//
//   UX:
//     UP/DOWN: navigate between options and the FLASH button
//     A: toggle option / confirm flash on FLASH button
//     X: quick Arcade preset + immediate flash
//     B: cancel
//
// ****************************************************************************

#include "../include.h"
#include "flash_dialog.h"

// Layout
#define DLG_TITLE_Y       14
#define DLG_INFO_Y        46
#define DLG_INFO_LINE_H   12
#define DLG_OPTS_Y        102
#define DLG_OPTS_LINE_H   13
#define DLG_BTN_GAP       12   // vertical spacing between last option and FLASH button
#define DLG_FOOTER_Y      218

// Number of options (= preserve, st7789, palette, cf2)
#define FLASH_OPT_COUNT   4
#define FLASH_BTN_INDEX   FLASH_OPT_COUNT  // = 4

// ---------------------------------------------------------------------------
// Format helpers
// ---------------------------------------------------------------------------

static void FormatSize(char* buf, u32 bytes)
{
    if (bytes >= 1024 * 1024) {
        DecNum(buf, bytes / (1024 * 1024), 0);
        strcat(buf, ".");
        char frac[8];
        DecNum(frac, (bytes / 102400) % 10, 0);
        strcat(buf, frac);
        strcat(buf, " MB");
    } else if (bytes >= 1024) {
        DecNum(buf, bytes / 1024, 0);
        strcat(buf, " KB");
    } else {
        DecNum(buf, bytes, 0);
        strcat(buf, " B");
    }
}

static void FormatTarget(char* buf, const Uf2Info* info)
{
    char addr_buf[16];
    addr_buf[0] = '0'; addr_buf[1] = 'x';
    for (int i = 0; i < 8; i++) {
        u32 nib = (info->target_addr >> ((7 - i) * 4)) & 0xF;
        addr_buf[2 + i] = (char)(nib < 10 ? '0' + nib : 'A' + nib - 10);
    }
    addr_buf[10] = 0;

    switch (info->kind) {
    case UF2_TARGET_RAM:   strcpy(buf, "RAM ");   break;
    case UF2_TARGET_FLASH: strcpy(buf, "Flash "); break;
    default:               strcpy(buf, "??? ");   break;
    }
    strcat(buf, "(");
    strcat(buf, addr_buf);
    strcat(buf, ")");
}

static const char* FamilyLabel(u32 family_id)
{
    switch (family_id) {
    case FAMILY_RP2040:       return "RP2040";
    case FAMILY_RP2350_ARM_S: return "RP2350-ARM-S";
    case FAMILY_RP2350_RISCV: return "RP2350-RISCV";
    case 0:                   return "(none)";
    default:                  return "unknown";
    }
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

static void RenderDialog(const char* path, const Uf2Info* info,
                         const FlashOptions* opts, int selected)
{
    DrawClear();

    // Title
    char title[40];
    if (info->kind == UF2_TARGET_RAM)
        strcpy(title, "Run from RAM");
    else
        strcpy(title, "Flash to memory");
    int title_x = (WIDTH - (int)strlen(title) * FONTW * 2) / 2;
    DrawText2(title, title_x, DLG_TITLE_Y, COL_YELLOW);

    // Info section
    int y = DLG_INFO_Y;
    DrawText("File:   ", 20, y, COL_GRAY);
    const char* fname = path;
    for (const char* p = path; *p; p++) if (*p == '/') fname = p + 1;
    DrawText(fname, 20 + 8 * FONTW, y, COL_WHITE);
    y += DLG_INFO_LINE_H;

    DrawText("Size:   ", 20, y, COL_GRAY);
    char size_buf[24];
    FormatSize(size_buf, info->total_size_bytes);
    DrawText(size_buf, 20 + 8 * FONTW, y, COL_WHITE);
    y += DLG_INFO_LINE_H;

    DrawText("Target: ", 20, y, COL_GRAY);
    char tgt_buf[40];
    FormatTarget(tgt_buf, info);
    DrawText(tgt_buf, 20 + 8 * FONTW, y, COL_WHITE);
    y += DLG_INFO_LINE_H;

    DrawText("Family: ", 20, y, COL_GRAY);
    DrawText(FamilyLabel(info->family_id), 20 + 8 * FONTW, y, COL_WHITE);

    // Options + FLASH button (only for Flash target)
    if (info->kind == UF2_TARGET_FLASH)
    {
        DrawRect(20, DLG_OPTS_Y - 8, WIDTH - 40, 1, COL_GRAY);
        DrawText("Options:", 20, DLG_OPTS_Y - 6, COL_LTGRAY);

        struct { const char* label; Bool flag; } items[] = {
            {"Preserve launcher",          opts->preserve_launcher},
            {"Patch ST7789 init",          opts->apply_st7789_init},
            {"Patch palette LUT",          opts->apply_palette_lut},
            {"Write CF2 config",           opts->apply_cf2},
        };

        for (int i = 0; i < FLASH_OPT_COUNT; i++)
        {
            int oy = DLG_OPTS_Y + 10 + i * DLG_OPTS_LINE_H;
            Bool is_sel = (i == selected);
            u16 col = is_sel ? COL_YELLOW : COL_WHITE;
            const char* arrow = is_sel ? ">" : " ";
            const char* mark  = items[i].flag ? "[x]" : "[ ]";
            if (is_sel) {
                DrawRect(18, oy - 1, WIDTH - 36, FONTH + 2, RGBTO16(40, 40, 0));
            }
            DrawText(arrow, 22, oy, col);
            DrawText(mark, 32, oy, col);
            DrawText(items[i].label, 32 + 4 * FONTW, oy, col);
        }

        // FLASH button — separated by a blank gap
        int by = DLG_OPTS_Y + 10 + FLASH_OPT_COUNT * DLG_OPTS_LINE_H + DLG_BTN_GAP;
        Bool btn_sel = (selected == FLASH_BTN_INDEX);
        const char* btn_label = " FLASH ";
        int btn_w = (int)strlen(btn_label) * FONTW + 12;
        int btn_x = (WIDTH - btn_w) / 2;
        u16 btn_bg, btn_fg;
        if (btn_sel) {
            btn_bg = COL_GREEN;
            btn_fg = COL_BLACK;
        } else {
            btn_bg = RGBTO16(0, 60, 0);
            btn_fg = COL_WHITE;
        }
        DrawRect(btn_x, by - 2, btn_w, FONTH + 4, btn_bg);
        DrawText(btn_label, btn_x + 6, by, btn_fg);
    }

    // Footer
    DrawRect(20, DLG_FOOTER_Y - 4, WIDTH - 40, 1, COL_GRAY);
    if (info->kind == UF2_TARGET_FLASH) {
        const char* hint = "A: OK   X: Arcade   Y: Raw   B: Cancel";
        DrawText(hint, (WIDTH - (int)strlen(hint) * FONTW) / 2,
                 DLG_FOOTER_Y, COL_LTGRAY);
    } else if (info->kind == UF2_TARGET_RAM) {
        const char* hint = "A: OK   B: Cancel";
        DrawText(hint, (WIDTH - (int)strlen(hint) * FONTW) / 2,
                 DLG_FOOTER_Y, COL_LTGRAY);
    } else {
        const char* hint = "Unknown target — B: Cancel";
        DrawText(hint, (WIDTH - (int)strlen(hint) * FONTW) / 2,
                 DLG_FOOTER_Y, COL_RED);
    }

    DispUpdate();
}

// ---------------------------------------------------------------------------
// Main entry
// ---------------------------------------------------------------------------

DialogResult FlashDialogShow(const char* path, const Uf2Info* info,
                             FlashOptions* opts)
{
    int total_rows = (info->kind == UF2_TARGET_FLASH)
                     ? FLASH_OPT_COUNT + 1   // 4 options + FLASH button
                     : 0;

    // Pre-select FLASH button so quickly pressing A confirms (= fast Arcade flow)
    int selected = (info->kind == UF2_TARGET_FLASH) ? FLASH_BTN_INDEX : 0;
    Bool need_redraw = True;

    while (1)
    {
        if (need_redraw) {
            RenderDialog(path, info, opts, selected);
            need_redraw = False;
        }

        u8 k = KeyGet();
        if (k == NOKEY) { WaitMs(20); continue; }

        switch (k)
        {
        case KEY_UP:
            if (total_rows > 0 && selected > 0) {
                selected--; need_redraw = True;
            }
            break;
        case KEY_DOWN:
            if (total_rows > 0 && selected < total_rows - 1) {
                selected++; need_redraw = True;
            }
            break;

        case KEY_A:
            // RAM target: A always confirms
            if (info->kind == UF2_TARGET_RAM) return DLG_PROCEED;
            // Flash target: A toggles checkboxes, confirms on FLASH button
            if (info->kind == UF2_TARGET_FLASH) {
                if (selected == FLASH_BTN_INDEX) {
                    return DLG_PROCEED;
                }
                if (selected < FLASH_OPT_COUNT) {
                    Bool* flags[] = {
                        &opts->preserve_launcher,
                        &opts->apply_st7789_init,
                        &opts->apply_palette_lut,
                        &opts->apply_cf2,
                    };
                    *flags[selected] = !*flags[selected];
                    need_redraw = True;
                }
            }
            break;

        case KEY_X:
            // Quick: Arcade preset + immediate flash
            if (info->kind == UF2_TARGET_FLASH) {
                opts->preserve_launcher = True;
                opts->apply_st7789_init = True;
                opts->apply_palette_lut = True;
                opts->apply_cf2         = True;
                return DLG_PROCEED;
            }
            break;

        case KEY_Y:
            // Quick: Raw preset (no Arcade patches, just preserve launcher).
            // Does NOT auto-flash — lets user inspect and confirm with A.
            if (info->kind == UF2_TARGET_FLASH) {
                opts->preserve_launcher = True;
                opts->apply_st7789_init = False;
                opts->apply_palette_lut = False;
                opts->apply_cf2         = False;
                need_redraw = True;
            }
            break;

        case KEY_B:
            return DLG_CANCEL;
        }
    }
}
