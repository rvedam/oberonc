#pragma once

// Hardware Abstraction Layer — freestanding Oberon runtime.
// Implemented per-platform in runtime/platform/<arch>/hal.cpp.
// All functions use C linkage so they can be called from assembly boot stubs.

#include "../compat_stdint.hpp"

// Project Oberon screen geometry, shared by framebuffer.cpp and every
// platform's display device (VBE mode, ramfb configuration).
constexpr int FB_W = 1024;
constexpr int FB_H = 768;

#ifdef __cplusplus
extern "C" {
#endif

void hal_init();             // called once by boot stub
void hal_putc(char c);       // write one character to output
int  hal_getc();             // read one char; -1 if none available
[[noreturn]] void hal_halt();

// Convert the 1bpp shadow framebuffer (at HAL_FramebufferBase()) to the
// real display device (VBE LFB on x86_64, ramfb on aarch64).
// Call once per frame after display.Mod has written pixels.
// No-op if no real display device was successfully configured.
void hal_display_flush();

// Shared (runtime/framebuffer.cpp): expand the 1bpp shadow buffer into an
// FB_W×FB_H XRGB8888 frame buffer (row stride FB_W pixels), redrawing only
// what changed since the previous call.
void fb_expand_xrgb(uint32_t* dst);

// Called by src/codegen.cpp's HAL pseudo-module (IMPORT HAL in Oberon
// source) — see Display.Mod/Kernel.Mod. HAL_FramebufferBase returns the
// shared 1bpp shadow buffer in BSS (runtime/framebuffer.cpp); the RISC5-era
// literal 0x0E7F00 is not usable RAM on either platform (PC BIOS area on
// x86_64, unmapped on QEMU virt).
// HAL_HeapOrigin/HAL_MemLimit are implemented once, shared, in
// oberon_runtime_bare.cpp — their correctness depends on being defined
// relative to the exact same static heap array Oberon_NEW bump-allocates
// from, so they must not be duplicated per-platform.
int64_t HAL_FramebufferBase();
int64_t HAL_HeapOrigin();
int64_t HAL_MemLimit();

// Devices used by the Project Oberon modules in place of RISC5 I/O registers.
// HAL_Time is per platform; the rest are shared (runtime/po_devices.cpp).
int64_t HAL_Time();                              // milliseconds since boot
int64_t HAL_Mouse();      // RISC5 mouse word: x[9:0], y[21:12], keys[26:24],
                          // bit 28 = keyboard scan code available
int64_t HAL_Keyboard();   // next PS/2 (set 2) scan code
void    HAL_ReadSector(int64_t sec, int64_t adr);   // 1 KiB file-system sector
void    HAL_WriteSector(int64_t sec, int64_t adr);

// Debug output helpers (shared, oberon_runtime_bare.cpp)
void hal_puts(const char* s);
void hal_put_hex(uint64_t v);

#ifdef __cplusplus
}
#endif
