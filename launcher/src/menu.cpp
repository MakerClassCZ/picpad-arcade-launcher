// ****************************************************************************
//
//             PicoPad Arcade Launcher — menu state machine
//
// ****************************************************************************

#include "../include.h"
#include "menu.h"
#include "file_picker.h"
#include "uf2_parser.h"
#include "flash_dialog.h"
#include "flasher.h"
#include "arcade_patches.h"
#if LAUNCHER_HAS_DEBUG_SHELL
#include "debug_shell.h"
#endif
#if LAUNCHER_HAS_USB_MSC
#include "usb_msc_screen.h"
#endif

// Menu labels
static const char* const MenuLabels[MENU_COUNT] = {
    "PLAY",
    "LOAD from SD",
#if LAUNCHER_HAS_USB_MSC
    "SD CARD READER (USB)",
#endif
#if LAUNCHER_HAS_DEBUG_SHELL
    "DEBUG SHELL (USB)",
#endif
    "REBOOT TO BOOTSEL",
};

// Layout constants
#define TITLE_Y       16
#define TITLE2_Y      36
#define DIVIDER_Y     64
#define MENU_START_Y  80
#define MENU_LINE_H   20
#define FOOTER_Y      210
#define ARROW_X       30
#define LABEL_X       60

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static int TextWidth(const char* s)
{
    return (int)StrLen(s) * FONTW;
}

static int TextWidth2(const char* s)
{
    return (int)StrLen(s) * FONTW * 2;
}

static void DrawCenteredText(const char* s, int y, u16 color)
{
    DrawText(s, (WIDTH - TextWidth(s)) / 2, y, color);
}

static void DrawCenteredText2(const char* s, int y, u16 color)
{
    DrawText2(s, (WIDTH - TextWidth2(s)) / 2, y, color);
}

// ---------------------------------------------------------------------------
// Game launch
// ---------------------------------------------------------------------------

Bool IsGameValid(void)
{
    volatile uint32_t* vt = (volatile uint32_t*)GAME_VECTOR_TABLE_ADDR;
    uint32_t sp    = vt[0];
    uint32_t entry = vt[1];

    // Erased flash (0xFFFFFFFF) or zeroed sector → no game
    if (sp == 0xFFFFFFFFu || sp == 0u)        return False;
    if (entry == 0xFFFFFFFFu || entry == 0u)  return False;

    // SP must point into RAM
    if (sp < RAM_BASE || sp > RAM_END)        return False;

    // Entry point must be in game flash region with Thumb bit set
    if (entry < GAME_FLASH_LO || entry >= GAME_FLASH_HI) return False;
    if ((entry & 1u) == 0u)                   return False;

    return True;
}

extern "C" void RuntimeTerm(void);
extern "C" void KeyWaitNoPressed(void);

// Magic written to WATCHDOG_SCRATCH[0] just before jumping to the game.
// On launcher boot, presence of this value means "we got here via a watchdog
// reset from a running game" (= soft reset, auto-relaunch). Power-on and
// RUN-pin reset zero the scratch registers (= hard reset → show menu).
#define LAUNCHER_BOOT_MAGIC  0xC0DEBA5Eu

// Hand off control to the flashed game via direct VTOR + jump.
//
// We CANNOT use watchdog vector boot — that path bypasses boot2, which
// means QSPI XIP is not set up, and the CPU can't fetch instructions from
// flash. Watchdog vector boot is intended for RAM-resident images.
//
// Direct jump preserves the XIP setup that our boot2 + launcher's crt0
// established at startup. The game's reset handler will reconfigure clocks
// and peripherals as needed.
void RunGame(void)
{
    // Default: vector table at 0x10000100 (= Arcade game or PicoLibSDK
    // loader, depending on what was flashed).
    u32 vector_addr = GAME_VECTOR_TABLE_ADDR;

    // PicoLibSDK app layout: loader at 0x10000100, actual app at 0x10008000.
    // If both have PPAD magic, launching the loader would just show its app
    // browser → user wants the app instead. Redirect to app at 0x10008000.
    const u32 PPAD_MAGIC = 0x44415050u;
    if (*(const volatile u32*)0x100001C0u == PPAD_MAGIC &&
        *(const volatile u32*)0x100080C0u == PPAD_MAGIC) {
        volatile uint32_t* app_vt = (volatile uint32_t*)0x10008000u;
        uint32_t app_sp = app_vt[0];
        uint32_t app_ep = app_vt[1];
        // Validate before redirecting — bad app vector would brick PLAY.
        if (app_sp >= RAM_BASE && app_sp <= RAM_END &&
            app_ep >= 0x10008000u && app_ep < GAME_FLASH_HI &&
            (app_ep & 1u)) {
            vector_addr = 0x10008000u;
        }
    }

    volatile uint32_t* vt = (volatile uint32_t*)vector_addr;
    uint32_t sp    = vt[0];
    uint32_t entry = vt[1];

    // Wait for any held keys (= so player doesn't bounce straight into menu
    // when game finishes if A is still down)
    KeyWaitNoPressed();

    // Mark this boot path as "came via launcher → game". If the game
    // watchdog-resets, the next launcher boot will see this magic and
    // auto-relaunch instead of showing menu.
    WATCHDOG_SCRATCH[0] = LAUNCHER_BOOT_MAGIC;

    // Shutdown peripherals — USB, UART, SysTick, display, SD, LED
    RuntimeTerm();

    // Disable IRQs while we tear down core state
    __asm volatile ("cpsid i" ::: "memory");

    // Clean ARMv6-M core state. PicoLibSDK app's RuntimeInit + DeviceInit
    // re-configures peripherals from scratch but leaves NVIC IRQ enables
    // and pending bits alone — stale enables from the launcher would let
    // wrong handlers fire on the new vector table.
    *(volatile uint32_t*)0xE000E180u = 0xFFFFFFFFu;  // NVIC_ICER0: disable all IRQs
    *(volatile uint32_t*)0xE000E280u = 0xFFFFFFFFu;  // NVIC_ICPR0: clear pending IRQs
    *(volatile uint32_t*)0xE000E010u = 0;            // SYST_CSR: stop SysTick
    *(volatile uint32_t*)0xE000E014u = 0;            // SYST_RVR: reset reload
    *(volatile uint32_t*)0xE000E018u = 0;            // SYST_CVR: reset current

    // Point VTOR at the chosen vector table. App's reset_handler sets it
    // again from its linker symbol, but for the small window between bx
    // and that store the CPU still uses VTOR for any pending exception.
    *(volatile uint32_t*)0xE000ED08u = vector_addr;

    // Re-enable IRQs. PicoLibSDK app's reset_handler (crt0_rp2040.S) does
    // not call cpsie i — it assumes a fresh power-on state where the I bit
    // is already clear. Our cpsid i above would otherwise mask SysTick
    // exception forever → KeyScan never runs → keys appear dead.
    __asm volatile ("dsb; isb" ::: "memory");
    __asm volatile ("cpsie i" ::: "memory");

    // Set MSP and jump (BX preserves Thumb bit). The game's reset handler
    // takes over from here.
    __asm volatile (
        "msr msp, %0\n"
        "bx  %1\n"
        :: "r"(sp), "r"(entry) : "memory"
    );

    while (1) {}  // unreachable
}

// ---------------------------------------------------------------------------
// Reboot to BOOTSEL — RP2040 ROM API call
// ---------------------------------------------------------------------------

void RebootToBootsel(void)
{
    // reset_usb_boot(gpio_mask, interface)
    //   gpio_mask = 0  → no GPIO indicator LED
    //   interface = 0  → both PICOBOOT and MSC interfaces (cold-boot default)
    reset_usb_boot(0, 0);
    // Never returns — RP2040 is now in BOOTSEL mode
    while (1) {}
}

// ---------------------------------------------------------------------------
// Stub screen helper — used by LOAD/USB/SETTINGS until M3/M4/M5
// ---------------------------------------------------------------------------

static void ShowStub(const char* title, const char* milestone)
{
    DrawClear();
    DrawCenteredText2(title, 30, COL_YELLOW);
    DrawCenteredText("Not implemented yet", 80, COL_LTGRAY);
    DrawCenteredText("Coming in milestone:", 100, COL_GRAY);
    DrawCenteredText(milestone, 120, COL_WHITE);
    DrawCenteredText("Press B to return", 200, COL_LTGRAY);
    DispUpdate();

    // Wait for KEY_B
    while (1)
    {
        u8 k = KeyGet();
        if (k == KEY_B) return;
    }
}

// ---------------------------------------------------------------------------
// Menu rendering
// ---------------------------------------------------------------------------

static void RenderMenu(int selected, Bool game_valid)
{
    DrawClear();

    // Title
    DrawCenteredText2("PicoPad Arcade", TITLE_Y,  COL_YELLOW);
    DrawCenteredText2("Launcher",       TITLE2_Y, COL_YELLOW);

    // Divider line under title
    DrawRect(40, DIVIDER_Y, WIDTH - 80, 1, COL_GRAY);

    // Menu items
    for (int i = 0; i < MENU_COUNT; i++)
    {
        int y = MENU_START_Y + i * MENU_LINE_H;

        u16 color;
        const char* arrow;

        if (i == selected)
        {
            color = COL_YELLOW;
            arrow = ">";
            // Highlight bar background
            DrawRect(LABEL_X - 6, y - 2, WIDTH - 2 * LABEL_X + 12, FONTH + 4,
                     RGBTO16(40, 40, 0));
        }
        else if (i == MENU_PLAY && !game_valid)
        {
            // Disabled state for PLAY when no game flashed
            color = COL_GRAY;
            arrow = " ";
        }
        else
        {
            color = COL_WHITE;
            arrow = " ";
        }

        DrawText(arrow, ARROW_X, y, color);
        DrawText(MenuLabels[i], LABEL_X, y, color);
    }

    // Footer divider + game status
    DrawRect(20, FOOTER_Y - 8, WIDTH - 40, 1, COL_GRAY);

    if (game_valid) {
        // PPAD magic at vector_table+0xC0 marks PicoLibSDK loader (= boot
        // variant at 0x10000100) and apps (= default variant at 0x10008000).
        const u32 PPAD_MAGIC = 0x44415050u;
        Bool has_loader = (*(const volatile u32*)0x100001C0u == PPAD_MAGIC);
        Bool has_app    = (*(const volatile u32*)0x100080C0u == PPAD_MAGIC);
        if (has_loader && has_app) {
            DrawText("App: PicoLibSDK", 20, FOOTER_Y, COL_GREEN);
        } else if (has_loader) {
            DrawText("Loader: PicoLibSDK (no app)", 20, FOOTER_Y, COL_YELLOW);
        } else {
            char name[32];
            if (ArcadeReadName(name, sizeof(name))) {
                char buf[48];
                strcpy(buf, "Game: ");
                strcat(buf, name);
                DrawText(buf, 20, FOOTER_Y, COL_GREEN);
            } else {
                DrawText("Game OK (unknown name)", 20, FOOTER_Y, COL_GREEN);
            }
        }
    } else {
        DrawText("No game flashed", 20, FOOTER_Y, COL_GRAY);
    }

    DispUpdate();
}

// ---------------------------------------------------------------------------
// Main menu loop
// ---------------------------------------------------------------------------

void MenuRun(void)
{
    // Soft reset (= watchdog reset from a previously-running game) →
    // auto-relaunch the game. Hard reset (power-on, RESET pin) clears
    // WATCHDOG_SCRATCH → falls through to the menu.
    Bool soft_reset = (WATCHDOG_SCRATCH[0] == LAUNCHER_BOOT_MAGIC);
    WATCHDOG_SCRATCH[0] = 0;
    if (soft_reset && IsGameValid()) {
        RunGame();  // never returns
    }

    int selected = MENU_PLAY;
    Bool need_redraw = True;

    while (1)
    {
        Bool game_valid = IsGameValid();

        // If selected is PLAY but no game, jump to next valid item on first entry
        if (selected == MENU_PLAY && !game_valid && need_redraw)
        {
            selected = MENU_LOAD;
        }

        if (need_redraw)
        {
            RenderMenu(selected, game_valid);
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
            do {
                selected = (selected - 1 + MENU_COUNT) % MENU_COUNT;
            } while (selected == MENU_PLAY && !game_valid);
            need_redraw = True;
            break;

        case KEY_DOWN:
            do {
                selected = (selected + 1) % MENU_COUNT;
            } while (selected == MENU_PLAY && !game_valid);
            need_redraw = True;
            break;

        case KEY_A:    // confirm / select
            switch (selected)
            {
            case MENU_PLAY:
                if (game_valid) RunGame(); // never returns
                break;
            case MENU_LOAD:
            {
                char selected_path[256];
                PickerResult r = FilePickerRun("/", "*.UF2",
                                               selected_path, sizeof(selected_path));
                if (r == PICKER_RESULT_SELECTED)
                {
                    // Parse UF2 metadata
                    Uf2Info info;
                    if (!Uf2ReadInfo(selected_path, &info))
                    {
                        DrawClear();
                        DrawText2("Invalid UF2", 60, 30, COL_RED);
                        DrawText("File is not a valid UF2 image", 10, 90, COL_WHITE);
                        DrawText("Press B to return", 10, 200, COL_GRAY);
                        DispUpdate();
                        while (KeyGet() != KEY_B) {}
                        need_redraw = True;
                        break;
                    }

                    // Defaults — Arcade preset
                    FlashOptions opts;
                    opts.preserve_launcher = True;
                    opts.apply_st7789_init = True;
                    opts.apply_palette_lut = True;
                    opts.apply_cf2         = True;

                    // Confirmation dialog
                    DialogResult d = FlashDialogShow(selected_path, &info, &opts);
                    if (d == DLG_PROCEED)
                    {
                        FlashResult fr;
                        if (info.kind == UF2_TARGET_RAM) {
                            fr = FlasherRunRam(selected_path, &info);
                            // RunRam never returns on success
                        } else if (info.kind == UF2_TARGET_FLASH) {
                            fr = FlasherWriteFlash(selected_path, &info, &opts);
                        } else {
                            fr = FLASH_ERR_INVALID;
                        }

                        // If we get here, flash failed — show error
                        const char* err;
                        switch (fr) {
                        case FLASH_ERR_OPEN:        err = "Cannot open UF2"; break;
                        case FLASH_ERR_INVALID:     err = "Invalid UF2 data"; break;
                        case FLASH_ERR_TOO_BIG:     err = "Image too big for RAM"; break;
                        case FLASH_ERR_PROTECTED:   err = "Overlaps protected region"; break;
                        case FLASH_ERR_WRITE:       err = "Flash write failed"; break;
                        case FLASH_ERR_INTERRUPTED: err = "Cancelled by user"; break;
                        default:                    err = "Unknown error"; break;
                        }
                        DrawClear();
                        DrawText2("Error", 110, 30, COL_RED);
                        DrawText(err, 10, 90, COL_WHITE);
                        DrawText("Press B to return", 10, 200, COL_GRAY);
                        DispUpdate();
                        while (KeyGet() != KEY_B) {}
                    }
                }
                need_redraw = True;
                break;
            }
#if LAUNCHER_HAS_USB_MSC
            case MENU_SD_READER:
                UsbMscScreenRun();  // never returns (does watchdog reset)
                break;
#endif
#if LAUNCHER_HAS_DEBUG_SHELL
            case MENU_DEBUG:
                DebugShellRun();  // never returns (does watchdog reset)
                break;
#endif
            case MENU_BOOTSEL:
                RebootToBootsel(); // never returns
                break;
            }
            break;

        default:
            break;
        }
    }
}
