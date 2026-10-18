// Project Oberon frame buffer, shared by all bare-metal platforms.
//
// Display.Mod draws into a 1bpp "shadow" buffer whose address it gets from
// HAL.FramebufferBase(): 1024×768 pixels, 128 bytes per row, bit k of each
// 32-bit word is pixel x+k, and row 0 is the BOTTOM line of the screen (the
// RISC5 video controller scans the buffer in reverse). Each platform's
// hal_display_flush hands its real 32-bit XRGB frame buffer to
// fb_expand_xrgb, which redraws only the 32-pixel words that changed.

#include "compat_stdint.hpp"
#include "platform/hal.hpp"

static constexpr int WORDS = FB_W / 32;   // FB_W, FB_H: platform/hal.hpp

alignas(16) static uint32_t shadow_fb[FB_H][WORDS];   // 96 KiB, zeroed BSS
static uint32_t drawn[FB_H][WORDS];                    // last expanded words
static bool     drawn_valid = false;
static uint32_t byte_pixels[256][8];                   // byte → 8 XRGB pixels

extern "C" int64_t HAL_FramebufferBase() {
    return reinterpret_cast<int64_t>(&shadow_fb[0][0]);
}

extern "C" void fb_expand_xrgb(uint32_t* dst) {
    if (!drawn_valid)
        for (int b = 0; b < 256; ++b)
            for (int bit = 0; bit < 8; ++bit)
                byte_pixels[b][bit] = (b >> bit) & 1 ? 0x00FFFFFFu : 0x00000000u;
    for (int row = 0; row < FB_H; ++row) {
        uint32_t* line = dst + static_cast<int64_t>(FB_H - 1 - row) * FB_W;
        for (int w = 0; w < WORDS; ++w) {
            uint32_t word = shadow_fb[row][w];
            if (drawn_valid && word == drawn[row][w]) continue;
            drawn[row][w] = word;
            uint32_t* px = line + w * 32;
            for (int byte = 0; byte < 4; ++byte, px += 8) {
                const uint32_t* pat = byte_pixels[(word >> (8 * byte)) & 0xFF];
                for (int k = 0; k < 8; ++k) px[k] = pat[k];
            }
        }
    }
    drawn_valid = true;
}
