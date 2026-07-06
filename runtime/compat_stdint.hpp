#pragma once
// On Apple hosts, Homebrew clang picks up macOS SDK libc++ headers even for
// bare-metal ELF targets (aarch64-none-elf). Those headers require OS thread
// APIs that do not exist in a -ffreestanding build and cause a compile error.
// CMakeLists.txt defines OBERON_BUILD_ON_APPLE for the apple-aarch64 kernel
// target; on Linux, the standard C++ headers work fine in freestanding mode.
#ifdef OBERON_BUILD_ON_APPLE
#  include <stdint.h>
#  include <stddef.h>
#else
#  include <cstdint>
#  include <cstddef>
#endif
