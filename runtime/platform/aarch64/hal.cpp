// AArch64 HAL: PL011 UART at 0x09000000 (QEMU virt machine)
// Compile with: -ffreestanding -O2  (requires AArch64 target)

#include "../hal.hpp"
#include "../../compat_stdint.hpp"

static void ramfb_init();

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

    ramfb_init();   // after the FPU enable: clang may use SIMD registers for copies
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
alignas(16) static uint8_t shadow_fb[FB_H][FB_W / 8]; // 98,304 bytes, zeroed by boot stub (BSS)

int64_t HAL_FramebufferBase() {
    return reinterpret_cast<int64_t>(&shadow_fb[0][0]);
}

// ---------------------------------------------------------------------------
// Timer: the generic timer's virtual counter (CNTVCT_EL0 / CNTFRQ_EL0)
// ---------------------------------------------------------------------------

int64_t HAL_Time() {
    uint64_t cnt = 0, frq = 1000;
#ifdef __aarch64__
    asm volatile("isb; mrs %0, cntvct_el0" : "=r"(cnt));
    asm volatile("mrs %0, cntfrq_el0" : "=r"(frq));
#endif
    return static_cast<int64_t>(cnt / (frq / 1000));
}

// ---------------------------------------------------------------------------
// Display: QEMU ramfb, configured through the fw_cfg DMA interface.
//
// fw_cfg on the virt machine is an MMIO device at 0x09020000 (selector at +8,
// DMA address at +16). Its registers and all DMA structures are big-endian.
// The guest owns the pixel memory; QEMU scans it out on every refresh, so
// hal_display_flush only has to expand the 1bpp shadow buffer into it.
// Run QEMU with `-device ramfb` (absent → display stays disabled).
// ---------------------------------------------------------------------------

static constexpr uintptr_t FW_CFG_BASE     = 0x09020000UL;
static constexpr uint16_t  FW_CFG_FILE_DIR = 0x19;
static constexpr uint32_t  DMA_CTL_ERROR = 0x01, DMA_CTL_READ = 0x02,
                           DMA_CTL_SELECT = 0x08, DMA_CTL_WRITE = 0x10;
static constexpr uint32_t  DRM_FORMAT_XRGB8888 = 0x34325258; // fourcc "XR24"

static inline uint16_t be16(uint16_t v) { return __builtin_bswap16(v); }
static inline uint32_t be32(uint32_t v) { return __builtin_bswap32(v); }
static inline uint64_t be64(uint64_t v) { return __builtin_bswap64(v); }

struct FwCfgDmaAccess { uint32_t control, length; uint64_t address; };

// Runs one fw_cfg DMA transfer on the item `key`; returns false on error.
static bool fw_cfg_dma(uint16_t key, uint32_t ctl, void* buf, uint32_t len) {
    alignas(16) static volatile FwCfgDmaAccess acc;
    acc.control = be32((static_cast<uint32_t>(key) << 16) | DMA_CTL_SELECT | ctl);
    acc.length  = be32(len);
    acc.address = be64(reinterpret_cast<uint64_t>(buf));
    asm volatile("" ::: "memory");
    *reinterpret_cast<volatile uint64_t*>(FW_CFG_BASE + 16) =
        be64(reinterpret_cast<uint64_t>(&acc));          // starts the transfer
    uint32_t c;
    while ((c = be32(acc.control)) & ~DMA_CTL_ERROR) {}  // cleared when done
    return (c & DMA_CTL_ERROR) == 0;
}

struct FwCfgFile { uint32_t size; uint16_t select, reserved; char name[56]; };

static bool streq(const char* a, const char* b) {
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}

alignas(4096) static uint32_t ramfb_pixels[FB_H][FB_W];   // XRGB8888, 3 MiB BSS
static bool ramfb_ok = false;

// Change detection and expansion for hal_display_flush: the shadow words
// last drawn, and each byte's 8 pixels (bit k = pixel k).
static uint32_t drawn[FB_H][FB_W / 32];
static bool     drawn_valid = false;
static uint32_t byte_pixels[256][8];

static void ramfb_init() {
    // The file directory: a big-endian entry count followed by the entries.
    static struct { uint32_t count; FwCfgFile files[64]; } dir;
    if (!fw_cfg_dma(FW_CFG_FILE_DIR, DMA_CTL_READ, &dir, sizeof dir)) return;
    uint32_t n = be32(dir.count);
    if (n > 64) n = 64;

    for (uint32_t i = 0; i < n; ++i) {
        if (!streq(dir.files[i].name, "etc/ramfb")) continue;
        struct __attribute__((packed)) {
            uint64_t addr; uint32_t fourcc, flags, width, height, stride;
        } static cfg;
        cfg.addr   = be64(reinterpret_cast<uint64_t>(&ramfb_pixels[0][0]));
        cfg.fourcc = be32(DRM_FORMAT_XRGB8888);
        cfg.flags  = 0;
        cfg.width  = be32(FB_W);
        cfg.height = be32(FB_H);
        cfg.stride = be32(FB_W * 4);
        ramfb_ok = fw_cfg_dma(be16(dir.files[i].select), DMA_CTL_WRITE, &cfg, sizeof cfg);
        for (int b = 0; b < 256; ++b)
            for (int bit = 0; bit < 8; ++bit)
                byte_pixels[b][bit] = (b >> bit) & 1 ? 0x00FFFFFFu : 0x00000000u;
        hal_puts(ramfb_ok ? "[ramfb: 1024x768]\n" : "[ramfb: configuration failed]\n");
        return;
    }
    hal_puts("[ramfb: none (run QEMU with -device ramfb)]\n");
}

// Project Oberon's frame buffer has y = 0 at the bottom (the RISC5 video
// controller scans it in reverse); bit k of each 32-bit word is pixel x+k.
// Only 32-pixel words that changed since the last flush are redrawn.
void hal_display_flush() {
    if (!ramfb_ok) return;
    for (int row = 0; row < FB_H; ++row) {
        const auto* src = reinterpret_cast<const uint32_t*>(shadow_fb[row]);
        uint32_t* line = ramfb_pixels[FB_H - 1 - row];
        for (int w = 0; w < FB_W / 32; ++w) {
            uint32_t word = src[w];
            if (drawn_valid && word == drawn[row][w]) continue;
            drawn[row][w] = word;
            uint32_t* dst = line + w * 32;
            for (int byte = 0; byte < 4; ++byte, dst += 8) {
                const uint32_t* px = byte_pixels[(word >> (8 * byte)) & 0xFF];
                for (int k = 0; k < 8; ++k) dst[k] = px[k];
            }
        }
    }
    drawn_valid = true;
}

// ---------------------------------------------------------------------------
// Exceptions (vector table in boot.S): report and stop
// ---------------------------------------------------------------------------

extern "C" [[noreturn]] void hal_exception(uint64_t kind, uint64_t esr, uint64_t elr,
                                           uint64_t far) {
    hal_puts("\n*** exception "); hal_put_hex(kind);
    hal_puts(" ESR="); hal_put_hex(esr);
    hal_puts(" ELR="); hal_put_hex(elr);
    hal_puts(" FAR="); hal_put_hex(far);
    hal_puts("\n");
    hal_halt();
}
