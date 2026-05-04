// ****************************************************************************
//
//             PicoPad Arcade Launcher — USB CDC debug shell
//
//   Connect to PicoPad as a USB CDC serial device (= COMx on Windows,
//   /dev/ttyACMx on Linux/macOS), 115200 8N1. Type 'help' for commands.
//
// ****************************************************************************

#include "../include.h"
#if LAUNCHER_HAS_DEBUG_SHELL
#include "debug_shell.h"
#include "uf2_parser.h"
#include "arcade_patches.h"
#include "boot2_embedded.h"
#include "flasher.h"

extern "C" void RuntimeTerm(void);
extern FRAMETYPE ALIGNED FrameBuf[FRAMESIZE];

// ---------------------------------------------------------------------------
// Output helpers (printf goes to USB CDC via PicoLibSDK USE_USB_STDIO)
// ---------------------------------------------------------------------------

static void p_str(const char* s)
{
    while (*s) UsbDevCdcWriteChar(*s++);
}

static void p_hex_byte(u8 b)
{
    char h[3];
    h[0] = (char)((b >> 4) < 10 ? '0' + (b >> 4) : 'a' + (b >> 4) - 10);
    h[1] = (char)((b & 0xF) < 10 ? '0' + (b & 0xF) : 'a' + (b & 0xF) - 10);
    h[2] = 0;
    p_str(h);
}

static void p_hex32(u32 v)
{
    p_str("0x");
    for (int i = 7; i >= 0; i--) {
        u32 nib = (v >> (i * 4)) & 0xF;
        char c = (char)(nib < 10 ? '0' + nib : 'a' + nib - 10);
        UsbDevCdcWriteChar(c);
    }
}

static void p_dec(u32 v)
{
    char buf[12];
    DecNum(buf, v, 0);
    p_str(buf);
}

static void p_eol(void)
{
    p_str("\r\n");
}

// ---------------------------------------------------------------------------
// Command parsing helpers
// ---------------------------------------------------------------------------

// Skip leading whitespace, return pointer
static char* skip_ws(char* s)
{
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

// Parse hex number, advance pointer. Returns 0 on no digits.
static u32 parse_hex(char** ps)
{
    char* s = skip_ws(*ps);
    // optional 0x prefix
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    u32 v = 0;
    int digits = 0;
    while (1) {
        char c = *s;
        u32 d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else break;
        v = (v << 4) | d;
        s++;
        digits++;
    }
    *ps = s;
    return digits ? v : 0;
}

// Parse decimal number
static u32 parse_dec(char** ps)
{
    char* s = skip_ws(*ps);
    u32 v = 0;
    int digits = 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (*s - '0');
        s++;
        digits++;
    }
    *ps = s;
    return digits ? v : 0;
}

// Get rest of line (= argument string)
static char* rest_of_line(char* s)
{
    return skip_ws(s);
}

// ---------------------------------------------------------------------------
// Hex dump
// ---------------------------------------------------------------------------

static void hexdump(const u8* data, u32 base, u32 len)
{
    u32 i = 0;
    while (i < len) {
        p_hex32(base + i);
        p_str(":  ");
        u32 n = (len - i < 16) ? (len - i) : 16;
        for (u32 k = 0; k < n; k++) {
            p_hex_byte(data[i + k]);
            p_str(" ");
        }
        // padding if last row not full
        for (u32 k = n; k < 16; k++) p_str("   ");
        p_str(" |");
        for (u32 k = 0; k < n; k++) {
            char c = data[i + k];
            UsbDevCdcWriteChar((c >= 32 && c < 127) ? c : '.');
        }
        p_str("|");
        p_eol();
        i += n;
    }
}

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------

static void cmd_help(void)
{
    p_str("Commands (all <len>/<addr>/<offset> are HEX):\r\n");
    p_str("  info                              launcher info\r\n");
    p_str("  ls <path>                         list directory\r\n");
    p_str("  filesize <path>                   show file size\r\n");
    p_str("  dump <addr> <len>                 hex+ascii memory\r\n");
    p_str("  erase <addr>                      erase 4 KB flash sector\r\n");
    p_str("  readraw <path> <offset> <len>     read raw bytes (max 256)\r\n");
    p_str("  readuf2 <path> <block>            read one UF2 block (decimal)\r\n");
    p_str("  scan <path> <hex bytes>           find sequence in file\r\n");
    p_str("  verify <path>                     check UF2 block_no's match\r\n");
    p_str("  flash <path>                      run full flasher on UF2\r\n");
    p_str("  bootsel                           reboot to USB MSC bootsel\r\n");
    p_str("  reset                             soft reset\r\n");
    p_str("  exit                              return to menu\r\n");
}

static void cmd_info(void)
{
    p_str("PicoPad Arcade Launcher debug shell\r\n");
    p_str("  flash size: ");
    p_hex32(0x10100000 - 0x10000000);
    p_eol();
    p_str("  game region: 0x10000100..0x100EFFFF\r\n");
    p_str("  launcher: 0x100F0000..0x100FBFFF\r\n");
    p_str("  CF2: 0x100FF000\r\n");
    if (*(const volatile u32*)0x100001C0u == 0x44415050u) {
        p_str("  flashed: PicoLibSDK app/loader (PPAD magic @ 0x100001C0)\r\n");
    } else {
        char name[40];
        if (ArcadeReadName(name, sizeof(name))) {
            p_str("  game name: ");
            p_str(name);
            p_eol();
        } else {
            p_str("  game name: (not found)\r\n");
        }
    }
}

static void cmd_dump(char* args)
{
    char* p = args;
    u32 addr = parse_hex(&p);
    u32 len = parse_hex(&p);
    if (len == 0) len = 64;
    if (len > 4096) len = 4096;
    p_str("Dump from ");
    p_hex32(addr);
    p_str(" length ");
    p_dec(len);
    p_eol();
    hexdump((const u8*)addr, addr, len);
}

static void cmd_erase(char* args)
{
    char* p = args;
    u32 addr = parse_hex(&p);
    u32 sec_addr = addr & ~0xFFFu;
    p_str("Erasing sector ");
    p_hex32(sec_addr);
    p_eol();
    FlashErase(sec_addr - 0x10000000u, 4096);
    p_str("Done\r\n");
}

static void cmd_readuf2(char* args)
{
    char* p = args;
    char* path = skip_ws(p);
    // find space
    char* sp = path;
    while (*sp && *sp != ' ' && *sp != '\t') sp++;
    if (!*sp) { p_str("Usage: readuf2 <path> <block>\r\n"); return; }
    *sp = 0;
    p = sp + 1;
    u32 block_no = parse_dec(&p);

    sFile file;
    FileInit(&file);
    if (!FileOpen(&file, path)) {
        p_str("Cannot open ");
        p_str(path); p_eol();
        return;
    }
    u8 buf[512];
    FileSeek(&file, block_no * 512);
    u32 n = FileRead(&file, buf, 512);
    FileClose(&file);

    p_str("Read ");
    p_dec(n);
    p_str(" bytes from block ");
    p_dec(block_no);
    p_str(" (file pos ");
    p_hex32(block_no * 512);
    p_str(")\r\n");
    if (n > 0) {
        // Parse UF2 header
        u32 m0 = ((u32)buf[0]) | ((u32)buf[1] << 8) | ((u32)buf[2] << 16) | ((u32)buf[3] << 24);
        u32 ta = ((u32)buf[12]) | ((u32)buf[13] << 8) | ((u32)buf[14] << 16) | ((u32)buf[15] << 24);
        u32 ps = ((u32)buf[16]) | ((u32)buf[17] << 8) | ((u32)buf[18] << 16) | ((u32)buf[19] << 24);
        u32 bn = ((u32)buf[20]) | ((u32)buf[21] << 8) | ((u32)buf[22] << 16) | ((u32)buf[23] << 24);
        p_str("  magic0=");  p_hex32(m0);
        p_str("  target="); p_hex32(ta);
        p_str("  psize=");  p_dec(ps);
        p_str("  block_no="); p_dec(bn);
        p_eol();
        hexdump(buf, 0, (n > 64 ? 64 : n));
    }
}

static void cmd_readraw(char* args)
{
    char* p = args;
    char* path = skip_ws(p);
    char* sp = path;
    while (*sp && *sp != ' ' && *sp != '\t') sp++;
    if (!*sp) { p_str("Usage: readraw <path> <offset> <len>\r\n"); return; }
    *sp = 0;
    p = sp + 1;
    u32 offset = parse_hex(&p);
    u32 len = parse_hex(&p);
    if (len == 0 || len > 256) len = 64;

    sFile file;
    FileInit(&file);
    if (!FileOpen(&file, path)) {
        p_str("Cannot open ");
        p_str(path); p_eol();
        return;
    }
    u8 buf[256];
    FileSeek(&file, offset);
    u32 n = FileRead(&file, buf, len);
    FileClose(&file);

    p_str("Read ");
    p_dec(n);
    p_str(" bytes from offset ");
    p_hex32(offset);
    p_eol();
    hexdump(buf, offset, n);
}

static void cmd_filesize(char* args)
{
    char* path = skip_ws(args);
    sFile file;
    FileInit(&file);
    if (!FileOpen(&file, path)) {
        p_str("Cannot open ");
        p_str(path); p_eol();
        return;
    }
    u32 size = FileSize(&file);
    FileClose(&file);
    p_str("File size: ");
    p_dec(size);
    p_str(" (");
    p_hex32(size);
    p_str(")\r\n");
}

static void cmd_ls(char* args)
{
    char* path = skip_ws(args);
    if (path[0] == 0) path = (char*)"/";
    sFile find;
    sFileInfo info;
    FileInit(&find);
    if (!FindOpen(&find, path)) {
        p_str("Cannot open ");
        p_str(path); p_eol();
        return;
    }
    p_str("Listing ");
    p_str(path);
    p_eol();
    int count = 0;
    while (FindNext(&find, &info, ATTR_DIR_MASK, "*")) {
        if (info.namelen == 0) continue;
        if (info.name[0] == '.') continue;
        if (info.attr & ATTR_DIR) p_str("  [D] ");
        else                       p_str("      ");
        for (int i = 0; i < info.namelen; i++) UsbDevCdcWriteChar(info.name[i]);
        if (!(info.attr & ATTR_DIR)) {
            p_str("\t");
            p_dec(info.size);
        }
        p_eol();
        count++;
    }
    FindClose(&find);
    p_str("Total: ");
    p_dec(count);
    p_str(" entries\r\n");
}

static void cmd_verify(char* args)
{
    char* path = skip_ws(args);
    sFile file;
    FileInit(&file);
    if (!FileOpen(&file, path)) {
        p_str("Cannot open ");
        p_str(path); p_eol();
        return;
    }
    u32 file_size = FileSize(&file);
    u32 expected_blocks = file_size / 512;
    p_str("Verifying ");
    p_str(path);
    p_str(" (");
    p_dec(expected_blocks);
    p_str(" blocks)...\r\n");

    u8 block[512];
    u32 mismatches = 0;
    for (u32 i = 0; i < expected_blocks; i++) {
        FileSeek(&file, i * 512);
        u32 n = FileRead(&file, block, 512);
        if (n != 512) {
            p_str("  ! Read failed at block ");
            p_dec(i); p_eol();
            break;
        }
        u32 m0 = ((u32)block[0]) | ((u32)block[1] << 8) | ((u32)block[2] << 16) | ((u32)block[3] << 24);
        if (m0 != 0x0A324655) continue;
        u32 bn = ((u32)block[20]) | ((u32)block[21] << 8) | ((u32)block[22] << 16) | ((u32)block[23] << 24);
        if (bn != i) {
            p_str("  MISMATCH at file pos block ");
            p_dec(i);
            p_str(": got block_no ");
            p_dec(bn);
            u32 ta = ((u32)block[12]) | ((u32)block[13] << 8) | ((u32)block[14] << 16) | ((u32)block[15] << 24);
            p_str(" target ");
            p_hex32(ta);
            p_eol();
            // dump first 32 bytes for inspection
            hexdump(block, i * 512, 32);
            mismatches++;
            if (mismatches > 5) {
                p_str("  ... (more mismatches, stopping)\r\n");
                break;
            }
        }
        // Heartbeat every 100 blocks
        if ((i % 100) == 0) {
            p_str("  block "); p_dec(i); p_str(" ok\r\n");
        }
    }
    FileClose(&file);
    p_str("Verify complete. Mismatches: ");
    p_dec(mismatches);
    p_eol();
}

// Scan a file for a hex byte sequence, print all match positions.
// Usage:  scan <path> <hex bytes>
//   e.g.  scan /arcade/falling.uf2 EF 03 03 80 02 CF 03
//   or    scan /arcade/falling.uf2 EF03038002CF03
static void cmd_scan(char* args)
{
    char* p = args;
    char* path = skip_ws(p);
    char* sp = path;
    while (*sp && *sp != ' ' && *sp != '\t') sp++;
    if (!*sp) { p_str("Usage: scan <path> <hex>\r\n"); return; }
    *sp = 0;
    p = sp + 1;

    // Parse needle (= sequence of hex bytes; whitespace optional)
    u8 needle[32];
    int needle_len = 0;
    char* h = skip_ws(p);
    while (*h && needle_len < 32) {
        // Skip whitespace between bytes
        while (*h == ' ' || *h == '\t') h++;
        if (!*h) break;
        u32 b = 0;
        int got = 0;
        while (*h && got < 2) {
            char c = *h;
            int d;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
            else break;
            b = (b << 4) | d;
            h++;
            got++;
        }
        if (got == 0) break;
        needle[needle_len++] = (u8)b;
    }
    if (needle_len == 0) { p_str("No hex bytes\r\n"); return; }

    p_str("Needle (");
    p_dec(needle_len);
    p_str(" B):");
    for (int i = 0; i < needle_len; i++) { p_str(" "); p_hex_byte(needle[i]); }
    p_eol();

    sFile file;
    FileInit(&file);
    if (!FileOpen(&file, path)) { p_str("Cannot open\r\n"); return; }
    u32 file_size = FileSize(&file);

    // Streaming scan with overlap so a needle on chunk boundary still matches
    static u8 chunk[4096 + 32];
    u32 base = 0;
    int matches = 0;
    int prev_tail = 0;

    while (base < file_size) {
        u32 to_read = (file_size - base < 4096) ? file_size - base : 4096;
        FileSeek(&file, base);
        u32 n = FileRead(&file, chunk + prev_tail, to_read);
        if (n == 0) break;
        u32 chunk_len = prev_tail + n;

        for (u32 i = 0; i + needle_len <= chunk_len; i++) {
            if (memcmp(chunk + i, needle, needle_len) == 0) {
                u32 abs_pos = (base - prev_tail) + i;
                p_str("  match @ ");
                p_hex32(abs_pos);
                // Translate to flash addr if mod 512 == [32..287] (= payload)
                u32 in_block = abs_pos & 511;
                if (in_block >= 32 && in_block < 288) {
                    u32 block_no = abs_pos >> 9;
                    u32 flash = 0x10000000u + block_no * 256u + (in_block - 32);
                    p_str("  -> flash ");
                    p_hex32(flash);
                }
                p_eol();
                matches++;
                if (matches >= 16) {
                    p_str("  (16+ matches, stopping)\r\n");
                    FileClose(&file);
                    return;
                }
            }
        }

        prev_tail = needle_len - 1;
        if (chunk_len > (u32)prev_tail) {
            memmove(chunk, chunk + chunk_len - prev_tail, prev_tail);
        }
        base += n;
        // Heartbeat every 64 KB
        if ((base & 0xFFFF) == 0) {
            p_str("  ... ");
            p_hex32(base);
            p_eol();
        }
    }
    FileClose(&file);
    p_str("Done. ");
    p_dec(matches);
    p_str(" matches\r\n");
}

// Run the full flasher (= same flow as menu LOAD → flash). Performs
// erase + sector-batched flash + scan-based patches + watchdog reset.
static void cmd_flash(char* args)
{
    char* path = skip_ws(args);
    if (!*path) { p_str("Usage: flash <path>\r\n"); return; }

    Uf2Info info;
    if (!Uf2ReadInfo(path, &info)) {
        p_str("Cannot parse UF2\r\n");
        return;
    }
    p_str("UF2: target="); p_hex32(info.target_addr);
    p_str(" blocks=");     p_dec(info.num_blocks);
    p_str(" size=");       p_dec(info.total_size_bytes);
    p_eol();

    if (info.kind != UF2_TARGET_FLASH) {
        p_str("Not a FLASH UF2 — refused\r\n");
        return;
    }

    FlashOptions opts;
    opts.preserve_launcher = True;
    opts.apply_st7789_init = True;
    opts.apply_palette_lut = True;
    opts.apply_cf2 = True;

    p_str("Flashing... (resets on done; press A on device to confirm)\r\n");
    FlashResult r = FlasherWriteFlash(path, &info, &opts);
    p_str("Flash result code: ");
    p_dec((u32)r);
    p_eol();
}

static void cmd_bootsel(void)
{
    p_str("Rebooting to BOOTSEL...\r\n");
    WaitMs(50);
    reset_usb_boot(0, 0);
    while (1) {}
}

static void cmd_reset(void)
{
    p_str("Resetting...\r\n");
    WaitMs(50);
    RuntimeTerm();
    WatchdogSetup(0, False);
    while (1) {}
}

// ---------------------------------------------------------------------------
// Shell main loop
// ---------------------------------------------------------------------------

#define LINE_MAX 128

static void process_line(char* line)
{
    char* p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == 0) return;

    // Find first word (= command)
    char* cmd = p;
    while (*p && *p != ' ' && *p != '\t') p++;
    char saved = *p;
    *p = 0;
    char* args = saved ? p + 1 : p;

    #define EQ(s) (StrComp(cmd, (s)) == 0)
    if (EQ("help") || EQ("?")) cmd_help();
    else if (EQ("info")) cmd_info();
    else if (EQ("dump")) cmd_dump(args);
    else if (EQ("erase")) cmd_erase(args);
    else if (EQ("readuf2")) cmd_readuf2(args);
    else if (EQ("readraw")) cmd_readraw(args);
    else if (EQ("filesize")) cmd_filesize(args);
    else if (EQ("ls")) cmd_ls(args);
    else if (EQ("verify")) cmd_verify(args);
    else if (EQ("scan")) cmd_scan(args);
    else if (EQ("flash")) cmd_flash(args);
    else if (EQ("bootsel")) cmd_bootsel();
    else if (EQ("reset")) cmd_reset();
    else if (EQ("exit") || EQ("quit")) {
        extern Bool g_shell_exit;
        g_shell_exit = True;
    }
    else {
        p_str("Unknown command: ");
        p_str(cmd); p_eol();
        p_str("Try 'help'\r\n");
    }
    #undef EQ
}

Bool g_shell_exit = False;

void DebugShellRun(void)
{
    g_shell_exit = False;

    // Initialize USB device with CDC class — without this the USB stack
    // never enumerates and host doesn't see a serial port.
    // (RuntimeInit only auto-inits this when USE_USB_STDIO=1, which we
    //  disable to allow direct UsbDevCdc* calls without printf wrappers.)
    UsbDevInit(&UsbDevCdcSetupDesc);

    // Show on display
    DrawClear();
    DrawText2("Debug shell", 70, 30, COL_YELLOW);
    DrawText("Connect via USB serial:", 20, 80, COL_LTGRAY);
    DrawText("  115200 8N1, COMx / ttyACMx", 20, 100, COL_WHITE);
    DrawText("Waiting for host...", 20, 130, COL_WHITE);
    DrawText("Press B on device to abort", 20, 200, COL_GRAY);
    DispUpdate();

    // Mount SD if not already
    DiskAutoMount();

    // Wait for host enumeration + DTR (terminal opens the port).
    // Show progress dots and allow B to bail out.
    {
        int dots = 0;
        while (!UsbDevCdcIsMounted()) {
            if (KeyGet() == KEY_B) return;
            WaitMs(100);
            if ((++dots % 5) == 0) {
                DrawTextBg(" ", 20 + (dots/5)*8 % 200, 150, COL_WHITE, COL_BLACK);
                DispUpdate();
            }
        }
    }

    DrawTextBg("Connected!                  ", 20, 130, COL_GREEN, COL_BLACK);
    DispUpdate();

    char line[LINE_MAX];
    int line_pos = 0;

    // Drain any garbage chars sent during enumeration
    while (UsbDevCdcReadReady() > 0) UsbDevCdcReadChar();

    p_str("\r\n--- PicoPad Arcade Launcher debug shell ---\r\n");
    p_str("Type 'help' for commands.\r\n> ");

    while (!g_shell_exit) {
        // Allow B button to exit
        if (KeyGet() == KEY_B) break;

        // Read available USB CDC chars
        while (UsbDevCdcReadReady() > 0) {
            char c = UsbDevCdcReadChar();
            if (c == '\r' || c == '\n') {
                if (line_pos > 0) {
                    line[line_pos] = 0;
                    p_eol();
                    process_line(line);
                    line_pos = 0;
                }
                p_str("> ");
            } else if (c == 0x7F || c == 0x08) {
                // backspace
                if (line_pos > 0) {
                    line_pos--;
                    p_str("\b \b");
                }
            } else if (c >= 32 && c < 127 && line_pos < LINE_MAX - 1) {
                line[line_pos++] = c;
                UsbDevCdcWriteChar(c);  // echo
            }
        }
        WaitMs(10);
    }

    p_str("\r\nResetting...\r\n");
    WaitMs(100);

    // Watchdog reset for clean USB state — same as MSC screen.
    // After reset, scratch[0] is not LAUNCHER_BOOT_MAGIC → menu shown.
    RuntimeTerm();
    WatchdogSetup(0, False);
    while (1) {}
}

#endif // LAUNCHER_HAS_DEBUG_SHELL
