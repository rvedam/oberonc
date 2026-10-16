// Project Oberon devices shared by all bare-metal platforms.
//
// The Oberon modules (Kernel.Mod, Input.Mod) were written for the RISC5
// board's memory-mapped SD-card, PS/2 keyboard and mouse registers. They now
// call these HAL functions instead:
//
//   HAL_ReadSector / HAL_WriteSector — a RAM disk holding a Project Oberon
//       file system (built by tools/mkpodisk.py, linked in by po_disk_image.S)
//   HAL_Keyboard / HAL_Mouse — characters typed on the serial console are
//       translated into the PS/2 scan codes Input.Mod decodes. There is no
//       pointing device yet: the mouse rests in the middle of the screen.
//
// HAL_Mouse is polled continuously by Oberon.Loop, so it also refreshes the
// display (hal_display_flush) at most every FLUSH_INTERVAL_MS.

#include "compat_stdint.hpp"
#include "platform/hal.hpp"

// ---------------------------------------------------------------------------
// RAM disk
// ---------------------------------------------------------------------------

// Writable copy of the disk image, placed in .data by po_disk_image.S.
extern "C" uint8_t po_disk_image[];
extern "C" uint8_t po_disk_image_end[];

static constexpr int64_t SECTOR_SIZE = 1024;

static void copy_bytes(uint8_t* dst, const uint8_t* src, int64_t n) {
    for (int64_t i = 0; i < n; ++i) dst[i] = src[i];
}

static bool sector_in_range(int64_t sec) {
    int64_t nsec = (po_disk_image_end - po_disk_image) / SECTOR_SIZE;
    if (sec >= 0 && sec < nsec) return true;
    hal_puts("RAM disk: sector out of range ");
    hal_put_hex(static_cast<uint64_t>(sec));
    hal_puts("\n");
    return false;
}

extern "C" void HAL_ReadSector(int64_t sec, int64_t adr) {
    auto* dst = reinterpret_cast<uint8_t*>(adr);
    if (!sector_in_range(sec)) {
        for (int64_t i = 0; i < SECTOR_SIZE; ++i) dst[i] = 0;
        return;
    }
    copy_bytes(dst, po_disk_image + sec * SECTOR_SIZE, SECTOR_SIZE);
}

extern "C" void HAL_WriteSector(int64_t sec, int64_t adr) {
    if (!sector_in_range(sec)) return;
    copy_bytes(po_disk_image + sec * SECTOR_SIZE, reinterpret_cast<const uint8_t*>(adr),
               SECTOR_SIZE);
}

// ---------------------------------------------------------------------------
// Keyboard: serial characters → PS/2 scan codes
// ---------------------------------------------------------------------------

// Input.Mod's translation table (KTab): index = scan code, +80H when Shift
// (or Ctrl) is held. Copied verbatim so the inverse mapping matches exactly.
static const uint8_t ktab[256] = {
    0x00,0x00,0x00,0x00,0x00,0x1A,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x09,0x60,0x00,
    0x00,0x00,0x00,0x00,0x00,0x71,0x31,0x00, 0x00,0x00,0x7A,0x73,0x61,0x77,0x32,0x00,
    0x00,0x63,0x78,0x64,0x65,0x34,0x33,0x00, 0x00,0x20,0x76,0x66,0x74,0x72,0x35,0x00,
    0x00,0x6E,0x62,0x68,0x67,0x79,0x36,0x00, 0x00,0x00,0x6D,0x6A,0x75,0x37,0x38,0x00,
    0x00,0x2C,0x6B,0x69,0x6F,0x30,0x39,0x00, 0x00,0x2E,0x2F,0x6C,0x3B,0x70,0x2D,0x00,
    0x00,0x00,0x27,0x00,0x5B,0x3D,0x00,0x00, 0x00,0x00,0x0D,0x5D,0x00,0x5C,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x7F,0x00,0x00,0x00,0x00,0x1B,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x09,0x7E,0x00,
    0x00,0x00,0x00,0x00,0x00,0x51,0x21,0x00, 0x00,0x00,0x5A,0x53,0x41,0x57,0x40,0x00,
    0x00,0x43,0x58,0x44,0x45,0x24,0x23,0x00, 0x00,0x20,0x56,0x46,0x54,0x52,0x25,0x00,
    0x00,0x4E,0x42,0x48,0x47,0x59,0x5E,0x00, 0x00,0x00,0x4D,0x4A,0x55,0x26,0x2A,0x00,
    0x00,0x3C,0x4B,0x49,0x4F,0x29,0x28,0x00, 0x00,0x3E,0x3F,0x4C,0x3A,0x50,0x5F,0x00,
    0x00,0x00,0x22,0x00,0x7B,0x2B,0x00,0x00, 0x00,0x00,0x0D,0x7D,0x00,0x7C,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x7F,0x00,0x00,0x00,0x00,0x1B,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

static constexpr uint8_t SC_RELEASE = 0xF0, SC_SHIFT = 0x12, SC_CTRL = 0x14;

static uint8_t  kbd_fifo[64];
static unsigned kbd_head, kbd_tail;   // head == tail: empty

static void kbd_push(uint8_t code) {
    unsigned next = (kbd_tail + 1) % sizeof(kbd_fifo);
    if (next == kbd_head) return;   // full: drop
    kbd_fifo[kbd_tail] = code;
    kbd_tail = next;
}

static void press(uint8_t modifier, uint8_t code) {
    if (modifier) kbd_push(modifier);
    kbd_push(code);
    kbd_push(SC_RELEASE); kbd_push(code);
    if (modifier) { kbd_push(SC_RELEASE); kbd_push(modifier); }
}

static void translate_char(uint8_t ch) {
    if (ch == 0x7F) ch = 0x08;   // terminals send DEL for Backspace
    if (ch == '\n') ch = 0x0D;
    for (int sc = 1; sc < 128; ++sc)
        if (ktab[sc] == ch) { press(0, static_cast<uint8_t>(sc)); return; }
    for (int sc = 1; sc < 128; ++sc)
        if (ktab[128 + sc] == ch) { press(SC_SHIFT, static_cast<uint8_t>(sc)); return; }
    // Control characters: Ctrl+letter (Input.Read maps them to ORD(ch) MOD 20H)
    if (ch >= 1 && ch <= 26)
        for (int sc = 1; sc < 128; ++sc)
            if (ktab[128 + sc] == 'A' + ch - 1) { press(SC_CTRL, static_cast<uint8_t>(sc)); return; }
}

static void poll_serial() {
    for (int c; (c = hal_getc()) != -1;) translate_char(static_cast<uint8_t>(c));
}

extern "C" int64_t HAL_Keyboard() {
    poll_serial();
    if (kbd_head == kbd_tail) return 0;
    uint8_t code = kbd_fifo[kbd_head];
    kbd_head = (kbd_head + 1) % sizeof(kbd_fifo);
    return code;
}

// ---------------------------------------------------------------------------
// Mouse (fixed position for now) + periodic display refresh
// ---------------------------------------------------------------------------

static constexpr int64_t FLUSH_INTERVAL_MS = 40;
static int64_t last_flush = -FLUSH_INTERVAL_MS;

extern "C" int64_t HAL_Mouse() {
    poll_serial();
    int64_t now = HAL_Time();
    if (now - last_flush >= FLUSH_INTERVAL_MS) {
        hal_display_flush();
        last_flush = now;
    }
    int64_t x = 512, y = 384, keys = 0;
    int64_t w = (keys << 24) | (y << 12) | x;
    if (kbd_head != kbd_tail) w |= int64_t(1) << 28;
    return w;
}
