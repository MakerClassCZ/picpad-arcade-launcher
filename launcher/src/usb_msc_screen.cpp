// ****************************************************************************
//
//             PicoPad Arcade Launcher — USB MSC SD card reader
//
// ****************************************************************************

#include "../include.h"
#if LAUNCHER_HAS_USB_MSC

#include "usb_msc_screen.h"
#include "sdk_usb_dev_msc.h"

extern "C" void RuntimeTerm(void);

// PicoLibSDK MSC source (vendored from TODO/) calls UsbXferComplete to
// re-trigger the driver's MscdXferComp completion handler from inside its
// own state machine — a tail-call pattern, not infinite recursion (state
// advances on each invocation). PicoLibSDK never defined the symbol, so we
// provide a thin stub. Note arg order: UsbXferComplete takes (ep, len,
// xres) but MscdXferComp wants (ep, xres, len).
extern "C" void UsbXferComplete(u8 epinx, u16 len, u8 xres)
{
    (void)MscdXferComp(epinx, xres, len);
}

// PicoLibSDK MSC source (TODO/) defines Init/Reset/Open/Ctrl/Comp but no
// Term. Provide an empty stub so the dispatcher's class driver array link.
extern "C" void MscdTerm(void)
{
    // No-op — interfaces are torn down on watchdog reset.
}

// ---------------------------------------------------------------------------
// USB descriptors
// ---------------------------------------------------------------------------

// MSC interface (single-LUN) — 1 itf, 2 bulk endpoints. PicoLibSDK has no
// pre-built USB_TEMP_MSCD macro, so we compose from USB_TEMP_ITF + EP.
#define USB_MSCD_ITF    0
#define USB_MSCD_EPOUT  USB_EPADDR(2, USB_DIR_OUT)
#define USB_MSCD_EPIN   USB_EPADDR(2, USB_DIR_IN)

#define USB_TEMP_MSCD(itf, str, epout, epin, epmax) \
    USB_TEMP_ITF((itf), 0, 2, USB_CLASS_MSC, 6 /*SCSI*/, 0x50 /*BBB*/, (str)), \
    USB_TEMP_EP((epout), USB_XFER_BULK, (epmax), 0), \
    USB_TEMP_EP((epin),  USB_XFER_BULK, (epmax), 0)

#define USB_TEMP_MSCD_LEN (9 + 7 + 7)

static const u8 UsbDevMscDescCfg[9 + USB_TEMP_MSCD_LEN] = {
    USB_TEMP_CFG(1, 1, 0, 9 + USB_TEMP_MSCD_LEN, 100),
    USB_TEMP_MSCD(USB_MSCD_ITF, 4, USB_MSCD_EPOUT, USB_MSCD_EPIN, USB_PACKET_MAX),
};

// PID override: PicoLibSDK USB_PID = 0x4101 when both CDC+MSC compiled in.
// We present only MSC here so use 0x4100 (= MSC bit only, no CDC bit) to
// keep Windows from confusing this with the previously-seen CDC composite
// device (which would have cached drivers tied to 0x4101).
#define USB_PID_MSC_ONLY  (0x4000 + (1 << 8))

// Standard single-LUN MSC: device class = 0 (per-interface), all class
// info goes into the interface descriptor.
static const sUsbDescDev UsbDevMscDescDev = {
    18,                  // size
    USB_DESC_DEVICE,     // type
    0x0200,              // USB 2.00
    0,                   // class — per-interface
    0,                   // subclass
    0,                   // protocol
    USB_PACKET_MAX,      // EP0 packet size
    0x2E8A,              // vendor (RPi)
    USB_PID_MSC_ONLY,    // product
    0x0100,              // device release
    1, 2, 3,             // string idxs (mfg, product, serial)
    1,                   // configurations
};

// PicoLibSDK sUsbDevSetupDesc.str_list expects `const char**` (= mutable
// pointer to const char*), not `const char* const*`, so we drop the
// outer const qualifier.
static const char* UsbDevMscDescStr[] = {
    "Raspberry Pi",      // 1: manufacturer
    "PicoPad",           // 2: product
    "00000000",          // 3: serial number
    "PicoPad SD",        // 4: configuration / volume label
};

static const sUsbDescCfg* UsbDescMscCfgList[1] =
    { (const sUsbDescCfg*)UsbDevMscDescCfg };

static const sUsbDevSetupDesc UsbDevMscSetupDesc = {
    .desc_dev      = &UsbDevMscDescDev,
    .desc_cfg_list = UsbDescMscCfgList,
    .desc_cfg_num  = 1,
    .desc_bos      = NULL,
    .desc_list     = NULL,
    .mounted_cb    = NULL,
    .suspend_cb    = NULL,
    .resume_cb     = NULL,
    .str_cb        = NULL,
    .str_list      = UsbDevMscDescStr,
    .str_num       = 4,
};

// ---------------------------------------------------------------------------
// MSC callbacks → PicoLibSDK SD lib
// ---------------------------------------------------------------------------

#define SECTOR_BYTES 512u

// Cached SD capacity, sampled once during UsbMscScreenRun setup.
// SDMediaSize() does a synchronous CSD-register read over SPI (~1 ms);
// calling it from msc_capacity_cb (= USB IRQ context) on every Get-Capacity
// request can starve other IRQs and stall the USB controller. Caching keeps
// the IRQ-side callback to a constant-time read.
static u32 g_sd_block_count = 0;

static int msc_read_cb(u8 lun, u32 lba, u32 off, void* buf, int bufsize)
{
    if (lun != 0 || off != 0 || (bufsize % SECTOR_BYTES) != 0) return -1;
    u8* dst = (u8*)buf;
    u32 sectors = (u32)bufsize / SECTOR_BYTES;
    for (u32 i = 0; i < sectors; i++) {
        if (!SDReadSect(lba + i, dst + i * SECTOR_BYTES)) return -1;
    }
    return bufsize;
}

static int msc_write_cb(u8 lun, u32 lba, u32 off, void* buf, int bufsize)
{
    if (lun != 0 || off != 0 || (bufsize % SECTOR_BYTES) != 0) return -1;
    const u8* src = (const u8*)buf;
    u32 sectors = (u32)bufsize / SECTOR_BYTES;
    for (u32 i = 0; i < sectors; i++) {
        if (!SDWriteSect(lba + i, src + i * SECTOR_BYTES)) return -1;
    }
    return bufsize;
}

static void msc_inquiry_cb(u8 lun, u8* vendor, u8* product, u8* version)
{
    memcpy(vendor,  "PicoPad ",         8);
    memcpy(product, "SD Card         ", 16);
    memcpy(version, "1.0 ",             4);
}

static Bool msc_ready_cb(u8 lun)
{
    return SDType != SD_NONE;
}

static void msc_capacity_cb(u8 lun, u32* block_count, u16* block_size)
{
    *block_count = g_sd_block_count;  // cached — see g_sd_block_count comment
    *block_size  = SECTOR_BYTES;
}

static Bool msc_writable_cb(u8 lun)
{
    return True;
}

// ---------------------------------------------------------------------------
// UI
// ---------------------------------------------------------------------------

static void DrawHeader(void)
{
    DrawClear();
    DrawText2("USB SD Reader", 50, 30, COL_YELLOW);
    DrawRect(40, 60, WIDTH - 80, 1, COL_GRAY);
}

static void DrawWaiting(int blink)
{
    DrawHeader();
    DrawText("Connect PicoPad to PC.",       20, 90,  COL_LTGRAY);
    DrawText("SD card will appear as drive", 20, 110, COL_LTGRAY);
    DrawText("'PicoPad SD'.",                20, 125, COL_LTGRAY);

    if (blink & 1) DrawText("Waiting for host...", 20, 170, COL_WHITE);

    DrawText("Press B to exit", 20, 215, COL_GRAY);
    DispUpdate();
}

static void DrawActive(u32 sectors)
{
    DrawHeader();
    DrawText("Connected.", 20, 90, COL_GREEN);
    char buf[64];
    strcpy(buf, "Capacity: ");
    char num[16];
    DecNum(num, sectors / 2048, 0);  // MB = sectors * 512 / 1M
    strcat(buf, num);
    strcat(buf, " MB");
    DrawText(buf, 20, 115, COL_LTGRAY);
    DrawText("Use PC to copy files.",      20, 145, COL_WHITE);
    DrawText("Eject from PC before exit.", 20, 165, COL_LTGRAY);
    DrawText("Display may freeze during",  20, 185, COL_GRAY);
    DrawText("transfer — that's normal.",  20, 200, COL_GRAY);
    DrawText("B: exit (after eject)", 20, 220, COL_GRAY);
    DispUpdate();
}

void UsbMscScreenRun(void)
{
    // Use DiskAutoMount to put SD into a known-good state — same path the
    // menu file picker uses successfully. If mount succeeds, the FAT layer
    // has done a full SD enumeration including capacity. Then we unmount
    // (= MSC layer wants raw sector access, not FAT).
    DrawHeader();
    DrawText("Initializing SD card...", 20, 90, COL_LTGRAY);
    DispUpdate();

    // Plausibility bounds for capacity (= sanity-check CSD parse).
    const u32 SD_MIN_SECTORS = 1024u;                  // > 512 KB
    const u32 SD_MAX_SECTORS = 256u * 1024u * 1024u;   // < 128 GB

    g_sd_block_count = 0;
    for (int attempt = 0; attempt < 3 && g_sd_block_count == 0; attempt++) {
        // Force teardown so each attempt starts clean
        DiskUnmount();
        SDDisconnect();
        WaitMs(50);

        // DiskAutoMount runs the full SDConnect + FAT mount sequence and
        // returns True only when SD is fully working.
        if (!DiskAutoMount()) { WaitMs(100); continue; }

        // SD is now in confirmed working state. Sample capacity (retried).
        for (int s = 0; s < 5; s++) {
            u32 sz = SDMediaSize();
            if (sz >= SD_MIN_SECTORS && sz <= SD_MAX_SECTORS) {
                g_sd_block_count = sz;
                break;
            }
            WaitMs(20);
        }

        // Unmount — MSC layer wants raw sectors, not FAT.
        DiskUnmount();
    }

    if (SDType == SD_NONE || g_sd_block_count == 0) {
        DrawClear();
        DrawText2("SD card not ready", (WIDTH - 17*16)/2, 80, COL_RED);
        DrawText("Insert card or check contacts.", 20, 130, COL_LTGRAY);
        char buf[40];
        strcpy(buf, "SDType=");
        char num[8]; DecNum(num, SDType, 0); strcat(buf, num);
        strcat(buf, " size=");
        DecNum(num, g_sd_block_count, 0); strcat(buf, num);
        DrawText(buf, 20, 150, COL_GRAY);
        DrawText("Press B to return", 20, 215, COL_GRAY);
        DispUpdate();
        while (KeyGet() != KEY_B) WaitMs(50);
        goto exit_msc;
    }

    // Wire MSC class driver callbacks BEFORE UsbDevInit. Note: read/write
    // callbacks fire in USB IRQ context and call SDReadSect/SDWriteSect
    // which spin on SPI — single-sector ops are fast enough (~1-2 ms) but
    // multi-sector chunks may starve other IRQs / main thread.
    MscdLun = 1;
    MscdRead10Cb     = msc_read_cb;
    MscdWrite10Cb    = msc_write_cb;
    MscdInquiryCb    = msc_inquiry_cb;
    MscdReadyCb      = msc_ready_cb;
    MscdGetCapacityCb = msc_capacity_cb;
    MscdIsWritableCb = msc_writable_cb;

    // Bring up USB device with MSC-only setup.
    UsbDevInit(&UsbDevMscSetupDesc);

    DrawWaiting(0);

    // Wait for host enumeration. Exit on B.
    {
        int blink = 0;
        while (1) {
            if (KeyGet() == KEY_B) goto exit_msc;
            WaitMs(200);
            DrawWaiting(++blink);
            if (MscdItf.ep_in != 0) break;  // MSC interface opened
        }
    }

    DrawActive(SDMediaSize());

    // Run loop — class driver runs from USB IRQ. We periodically refresh
    // display so a long IRQ-context SD I/O burst doesn't appear as a hang.
    {
        u32 tick = 0;
        while (1) {
            if (KeyGet() == KEY_B) break;
            if (!UsbIsMounted()) break;  // host unplugged
            WaitMs(50);
            // Heartbeat: blink a small marker so user can see launcher is alive
            if ((++tick & 0xF) == 0) {
                DrawTextBg((tick & 0x10) ? "*" : " ",
                           WIDTH - 16, 30, COL_GREEN, COL_BLACK);
                DispUpdate();
            }
        }
    }

exit_msc:
    // Watchdog reset for clean USB state. After reset, scratch[0] is not
    // LAUNCHER_BOOT_MAGIC → menu shown (no auto-launch).
    DrawClear();
    DrawText2("Restarting...", 80, 100, COL_LTGRAY);
    DispUpdate();
    WaitMs(300);
    RuntimeTerm();
    WatchdogSetup(0, False);
    while (1) {}
}

#endif // LAUNCHER_HAS_USB_MSC
