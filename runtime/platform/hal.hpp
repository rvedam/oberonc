#pragma once

// Hardware Abstraction Layer — freestanding Oberon runtime.
// Implemented per-platform in runtime/platform/<arch>/hal.cpp.
// All functions use C linkage so they can be called from assembly boot stubs.

#include "../compat_stdint.hpp"

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

// Called by src/codegen.cpp's HAL pseudo-module (IMPORT HAL in Oberon
// source) — see Display.Mod/Kernel.Mod. HAL_FramebufferBase is genuinely
// per-platform (implemented in platform/<arch>/hal.cpp: x86_64 returns the
// existing SHADOW_BASE; aarch64 returns a real static buffer, since aarch64
// has no display device backing the RISC5-era literal 0x0E7F00 at all).
// HAL_HeapOrigin/HAL_MemLimit are implemented once, shared, in
// oberon_runtime_bare.cpp — their correctness depends on being defined
// relative to the exact same static heap array Oberon_NEW bump-allocates
// from, so they must not be duplicated per-platform.
int64_t HAL_FramebufferBase();
int64_t HAL_HeapOrigin();
int64_t HAL_MemLimit();

#ifdef __cplusplus
}
#endif
