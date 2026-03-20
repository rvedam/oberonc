#pragma once
#include <cstdint>

// Hardware Abstraction Layer — freestanding Oberon runtime.
// Implemented per-platform in runtime/platform/<arch>/hal.cpp.
// All functions use C linkage so they can be called from assembly boot stubs.

#ifdef __cplusplus
extern "C" {
#endif

void hal_init();             // called once by boot stub
void hal_putc(char c);       // write one character to output
int  hal_getc();             // read one char; -1 if none available
[[noreturn]] void hal_halt();

#ifdef __cplusplus
}
#endif
