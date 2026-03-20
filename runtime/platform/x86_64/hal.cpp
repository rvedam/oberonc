// x86-64 HAL: VGA text buffer + COM1 UART
// Compile with: -ffreestanding -mno-red-zone -O2

#include "../hal.hpp"
#include <cstdint>

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
// HAL implementation
// ---------------------------------------------------------------------------

void hal_init() {
    uart_init();
    // Initialise x87 FPU (needed for double arithmetic in Out_Real)
    asm volatile("fninit");
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
