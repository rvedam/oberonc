// AArch64 HAL: PL011 UART at 0x09000000 (QEMU virt machine)
// Compile with: -ffreestanding -O2  (requires AArch64 target)

#include "../hal.hpp"
#include "../../compat_stdint.hpp"

// ---------------------------------------------------------------------------
// PL011 UART register offsets
// ---------------------------------------------------------------------------

static constexpr uintptr_t UART_BASE = 0x09000000UL;
static constexpr uint32_t  UART_DR   = 0x000; // data register
static constexpr uint32_t  UART_FR   = 0x018; // flag register
static constexpr uint32_t  UART_IBRD = 0x024; // integer baud-rate divisor
static constexpr uint32_t  UART_FBRD = 0x028; // fractional baud-rate divisor
static constexpr uint32_t  UART_LCR  = 0x02C; // line control
static constexpr uint32_t  UART_CR   = 0x030; // control register

static constexpr uint32_t FR_RXFE = (1u << 4); // receive FIFO empty
static constexpr uint32_t FR_TXFF = (1u << 5); // transmit FIFO full

static volatile uint32_t* uart_reg(uint32_t off) {
    return reinterpret_cast<volatile uint32_t*>(UART_BASE + off);
}

// ---------------------------------------------------------------------------
// HAL implementation
// ---------------------------------------------------------------------------

void hal_init() {
    // Disable UART
    *uart_reg(UART_CR) = 0;

    // Set baud rate: 38400 for 24 MHz reference clock
    // IBRD = 39, FBRD = 1  (24000000 / (16 * 38400) ≈ 39.06)
    *uart_reg(UART_IBRD) = 39;
    *uart_reg(UART_FBRD) = 1;

    // 8N1, enable FIFOs (WLEN=11b, FEN=1)
    *uart_reg(UART_LCR) = (3u << 5) | (1u << 4);

    // Enable UART, TX, RX
    *uart_reg(UART_CR) = (1u << 9) | (1u << 8) | (1u << 0);

#ifdef __aarch64__
    // Enable FPU/SIMD at EL1: CPACR_EL1 FPEN = 11b (bits 21:20)
    uint64_t cpacr;
    asm volatile("mrs %0, cpacr_el1" : "=r"(cpacr));
    cpacr |= (3UL << 20);
    asm volatile("msr cpacr_el1, %0" : : "r"(cpacr));
    asm volatile("isb");
#endif
}

void hal_putc(char c) {
    // Spin while transmit FIFO is full
    while (*uart_reg(UART_FR) & FR_TXFF) {}
    *uart_reg(UART_DR) = static_cast<uint32_t>(static_cast<uint8_t>(c));
}

int hal_getc() {
    if (*uart_reg(UART_FR) & FR_RXFE)
        return -1;
    return static_cast<int>(*uart_reg(UART_DR) & 0xFF);
}

[[noreturn]] void hal_halt() {
#ifdef __aarch64__
    for (;;) asm volatile("wfi");
#else
    for (;;) {} // host build: busy loop (never called in practice)
#endif
}

// Shadow framebuffer: 1bpp, 1024×768 packed. Stride = 1024/8 = 128 bytes.
// Same layout Display.Mod already assumes on x86_64 (see
// platform/x86_64/hal.cpp's SHADOW_BASE comment). Unlike x86_64, aarch64 has
// no real backing memory at the RISC5-era literal address 0x0E7F00 (QEMU
// virt's RAM starts at 0x40000000), so this lives in our own BSS instead —
// Display.Mod now reads the address via HAL.FramebufferBase() rather than a
// hardcoded literal (see src/codegen.cpp's HAL pseudo-module), so this is a
// transparent substitution from the Oberon source's point of view.
static constexpr int FB_W = 1024, FB_H = 768;
static uint8_t shadow_fb[FB_H][FB_W / 8]; // 98,304 bytes, zeroed by boot stub (BSS)

int64_t HAL_FramebufferBase() {
    return reinterpret_cast<int64_t>(&shadow_fb[0][0]);
}

// TODO(Bug 6 / Step 9): drive a real QEMU `virt` display device (ramfb) and
// blit shadow_fb into it here. Until then this is a no-op, same as before —
// Display.Mod can now write to shadow_fb without faulting, but nothing
// reads it yet.
void hal_display_flush() {}
