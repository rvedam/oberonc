// x86-64 HAL: VGA text buffer + COM1 UART + Bochs VBE display
// Compile with: -ffreestanding -mno-red-zone -O2

#include "../hal.hpp"
#include "../../compat_stdint.hpp"

// ---------------------------------------------------------------------------
// Port I/O helpers
// ---------------------------------------------------------------------------

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
static inline void outw(uint16_t port, uint16_t val) {
    asm volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
static inline void outl(uint16_t port, uint32_t val) {
    asm volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    asm volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// ---------------------------------------------------------------------------
// COM1 UART (0x3F8), 38400 baud 8N1
// ---------------------------------------------------------------------------

static constexpr uint16_t COM1 = 0x3F8;

static void uart_init() {
    outb(COM1 + 1, 0x00); // disable interrupts
    outb(COM1 + 3, 0x80); // enable DLAB
    outb(COM1 + 0, 0x03); // divisor low byte  (115200 / 3 = 38400)
    outb(COM1 + 1, 0x00); // divisor high byte
    outb(COM1 + 3, 0x03); // 8N1, DLAB off
    outb(COM1 + 2, 0xC7); // enable+clear FIFOs, 14-byte threshold
    outb(COM1 + 4, 0x0B); // RTS+DTR+OUT2
}

static void uart_putc(char c) {
    // wait for transmit holding register empty (LSR bit 5)
    while (!(inb(COM1 + 5) & 0x20)) {}
    outb(COM1, static_cast<uint8_t>(c));
}

// ---------------------------------------------------------------------------
// VGA text buffer (80×25, attribute 0x07 = light grey on black)
// ---------------------------------------------------------------------------

static constexpr uint32_t VGA_ADDR  = 0xB8000;
static constexpr int      VGA_COLS  = 80;
static constexpr int      VGA_ROWS  = 25;

static int vga_col = 0;
static int vga_row = 0;

static volatile uint16_t* vga() {
    return reinterpret_cast<volatile uint16_t*>(
        static_cast<uintptr_t>(VGA_ADDR));
}

static void vga_scroll() {
    volatile uint16_t* buf = vga();
    // copy rows 1..24 → 0..23
    for (int r = 0; r < VGA_ROWS - 1; ++r)
        for (int c = 0; c < VGA_COLS; ++c)
            buf[r * VGA_COLS + c] = buf[(r + 1) * VGA_COLS + c];
    // blank last row
    for (int c = 0; c < VGA_COLS; ++c)
        buf[(VGA_ROWS - 1) * VGA_COLS + c] = 0x0720; // space, attr 0x07
}

static void vga_putc(char c) {
    volatile uint16_t* buf = vga();
    if (c == '\n') {
        vga_col = 0;
        ++vga_row;
    } else {
        buf[vga_row * VGA_COLS + vga_col] =
            static_cast<uint16_t>(0x0700 | static_cast<uint8_t>(c));
        ++vga_col;
        if (vga_col >= VGA_COLS) { vga_col = 0; ++vga_row; }
    }
    if (vga_row >= VGA_ROWS) { vga_scroll(); vga_row = VGA_ROWS - 1; }
}

// ---------------------------------------------------------------------------
// PCI config-space access (CAM I/O mechanism, ports 0xCF8 / 0xCFC)
// ---------------------------------------------------------------------------

static uint32_t pci_read32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg) {
    uint32_t addr = (uint32_t(1) << 31)
                  | (uint32_t(bus)  << 16)
                  | (uint32_t(dev)  << 11)
                  | (uint32_t(func) <<  8)
                  | (reg & 0xFC);
    outl(0xCF8, addr);
    return inl(0xCFC);
}

// Scan all PCI functions on bus 0 for a device matching vendor:device.
// Returns the value of BAR0 (bits 31..4 = base address) or 0 if not found.
static uint32_t pci_find_bar0(uint16_t vendor, uint16_t device) {
    for (uint8_t dev = 0; dev < 32; ++dev) {
        uint32_t id = pci_read32(0, dev, 0, 0x00);
        if ((id & 0xFFFF) == vendor && ((id >> 16) & 0xFFFF) == device)
            return pci_read32(0, dev, 0, 0x10) & ~uint32_t(0xF); // BAR0, strip flags
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Bochs VBE (I/O ports 0x1CE / 0x1CF) — available on QEMU -vga std
//
// Display.Mod draws into the shared 1bpp shadow buffer (framebuffer.cpp).
// We set a 1024×768×32bpp VBE mode, find its linear framebuffer through PCI,
// and let fb_expand_xrgb copy changes into it from hal_display_flush().
// ---------------------------------------------------------------------------

static constexpr uint16_t VBE_IDX  = 0x1CE;
static constexpr uint16_t VBE_DATA = 0x1CF;

static inline void vbe_write(uint16_t idx, uint16_t val) {
    outw(VBE_IDX, idx);
    outw(VBE_DATA, val);
}
static inline uint16_t vbe_read(uint16_t idx) {
    outw(VBE_IDX, idx);
    return inw(VBE_DATA);
}

// The 32bpp VBE linear framebuffer (null if VBE unavailable)
static uint32_t* vbe_lfb = nullptr;

static void vbe_init() {
    // Verify the Bochs VBE ID tag is present (0xB0C0..0xB0C5)
    uint16_t id = vbe_read(0x0);
    if ((id & 0xFFF0) != 0xB0C0) return; // no VBE device

    vbe_write(0x4, 0x0);      // disable VBE
    vbe_write(0x1, FB_W);     // XRES
    vbe_write(0x2, FB_H);     // YRES
    vbe_write(0x3, 32);       // BPP = 32 (XRGB8888)
    vbe_write(0x4, 0x41);     // enable + linear framebuffer mode

    // Locate the VBE LFB via PCI BAR0 of the Bochs VGA device (0x1234:0x1111)
    uint32_t bar0 = pci_find_bar0(0x1234, 0x1111);
    if (bar0 == 0) return;

    vbe_lfb = reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(bar0));
}

// ---------------------------------------------------------------------------
// Timer: the TSC, calibrated once against the PIT (1.193182 MHz)
// ---------------------------------------------------------------------------

static uint64_t tsc_per_ms = 1;
static uint64_t tsc_boot;                     // TSC at timer_init: HAL_Time's zero

static inline uint64_t rdtsc() {
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return (uint64_t(hi) << 32) | lo;
}

static uint16_t pit_read() {
    outb(0x43, 0x00);                         // latch channel 0
    uint8_t lo = inb(0x40), hi = inb(0x40);
    return static_cast<uint16_t>(lo | (hi << 8));
}

static void timer_init() {
    outb(0x43, 0x34);                         // channel 0, lo/hi byte, mode 2
    outb(0x40, 0x00); outb(0x40, 0x00);       // reload 65536 (~55 ms period)
    // Count 11932 PIT ticks (10 ms) while watching the TSC; the counter runs
    // down, so elapsed ticks are (previous - current) mod 65536. A PIT that
    // never counts (absent or gated off) must not hang the boot: give up
    // after MAX_POLLS reads (each is ~1 µs of port I/O, so ~1 s) and assume
    // a 1 GHz TSC.
    constexpr uint32_t TICKS = 11932;
    constexpr uint32_t MAX_POLLS = 1000000;
    uint16_t prev = pit_read();
    uint32_t elapsed = 0;
    uint64_t t0 = rdtsc();
    for (uint32_t polls = 0; elapsed < TICKS && polls < MAX_POLLS; ++polls) {
        uint16_t now = pit_read();
        elapsed += static_cast<uint16_t>(prev - now);
        prev = now;
    }
    uint64_t t1 = rdtsc();
    tsc_boot = t1;
    if (elapsed < TICKS) {
        hal_puts("[timer: PIT not counting; assuming a 1 GHz TSC]\n");
        tsc_per_ms = 1000000;
        return;
    }
    tsc_per_ms = (t1 - t0) * 1193182 / (uint64_t(elapsed) * 1000);
    if (tsc_per_ms == 0) tsc_per_ms = 1;
}

int64_t HAL_Time() { return static_cast<int64_t>((rdtsc() - tsc_boot) / tsc_per_ms); }

// ---------------------------------------------------------------------------
// HAL implementation
// ---------------------------------------------------------------------------

static void uart_print_hex32(uint32_t v) {
    uart_putc('0'); uart_putc('x');
    for (int i = 28; i >= 0; i -= 4) {
        uint8_t n = (v >> i) & 0xF;
        uart_putc(n < 10 ? '0'+n : 'A'+n-10);
    }
}

void hal_init() {
    uart_init();
    // Initialise x87 FPU (needed for double arithmetic in Out_Real)
    asm volatile("fninit");
    timer_init();
    vbe_init();
    // Report VBE status on serial for debugging
    if (vbe_lfb) {
        uart_putc('['); uart_putc('V'); uart_putc('B'); uart_putc('E');
        uart_putc(':');
        uart_print_hex32(static_cast<uint32_t>(
            reinterpret_cast<uintptr_t>(vbe_lfb)));
        uart_putc(']'); uart_putc('\n');
    } else {
        const char* msg = "[VBE:none]\n";
        while (*msg) uart_putc(*msg++);
    }
}

void hal_putc(char c) {
    vga_putc(c);
    uart_putc(c);
}

int hal_getc() {
    // Poll COM1 LSR bit 0 (data ready)
    if (inb(COM1 + 5) & 0x01)
        return static_cast<int>(inb(COM1));
    return -1;
}

[[noreturn]] void hal_halt() {
    asm volatile("cli");
    for (;;) asm volatile("hlt");
}

// ---------------------------------------------------------------------------
// Display and exceptions
// ---------------------------------------------------------------------------

// The shadow buffer's layout and change tracking live in framebuffer.cpp.
void hal_display_flush() {
    if (vbe_lfb) fb_expand_xrgb(vbe_lfb);
}

// Called by the IDT stubs in boot.S for every CPU exception: report and stop.
extern "C" [[noreturn]] void hal_exception(uint64_t vector, uint64_t error,
                                           uint64_t rip, uint64_t cr2) {
    hal_puts("\n*** exception "); hal_put_hex(vector);
    hal_puts(" error="); hal_put_hex(error);
    hal_puts(" RIP="); hal_put_hex(rip);
    hal_puts(" CR2="); hal_put_hex(cr2);
    hal_puts("\n");
    hal_halt();
}
