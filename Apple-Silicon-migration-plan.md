# Apple Silicon Migration Plan

## Overview

Extend the oberonc build system to support Apple Silicon (arm64) as a first-class target for both the host compiler build and the bare-metal AArch64 kernel, then compile a full Project Oberon kernel image from the sources in `project-oberon/`.

---

## Part 1 — Host Build & Test Suite (Completed)

### Problem
CMakeLists.txt required LLVM 14, which is not available on macOS. Apple Silicon Macs ship with Homebrew LLVM 18. Three LLVM API breakages prevented compilation.

### Changes Made

**`CMakeLists.txt`** — LLVM auto-detection
- Replaced `find_package(LLVM 14 REQUIRED)` with a filesystem-probe loop that tries LLVM 18 (`/opt/homebrew/opt/llvm@18/lib/cmake/llvm`) then LLVM 14 (`/usr/lib/llvm-14/lib/cmake/llvm`), setting `LLVM_DIR` before calling `find_package(LLVM REQUIRED CONFIG)`.

**`src/codegen.cpp`** — LLVM 14 → 18 API fixes
| Old (LLVM 14) | New (LLVM 18) |
|---|---|
| `#include <llvm/Support/Host.h>` | `#include <llvm/TargetParser/Host.h>` (via `__has_include`) |
| `llvm::Optional<llvm::Reloc::Model>` | `std::optional<llvm::Reloc::Model>` |
| `llvm::CGFT_ObjectFile` | `llvm::CodeGenFileType::ObjectFile` |
| `fnptrLLVM->getPointerElementType()` | new `toLLVMFunctionType(procTy)` helper |

**`src/codegen.hpp`** — added `toLLVMFunctionType()` declaration

**`src/codegen_gen.cpp`** — replaced two `getPointerElementType()` call sites

### Result
All 92 tests pass on Apple Silicon with LLVM 18.

---

## Part 2 — Apple Silicon Bare-Metal Kernel Target (Completed)

### Problem
The existing `aarch64` KERNEL_ARCH target uses `aarch64-linux-gnu-g++` (unavailable on macOS). A native Apple Silicon toolchain path was needed.

### Changes Made

**`CMakeLists.txt`** — new `apple-aarch64` KERNEL_ARCH
- Uses `clang++ --target=aarch64-none-elf` from Homebrew LLVM 18
- Uses `ld.lld` (ELF linker, part of LLVM 18) instead of GNU ld
- Uses `llvm-ar` for archiving
- Compiles `oberon_runtime_bare.cpp` + `platform/aarch64/hal.cpp` + `boot/aarch64/boot.S`
- Adds `run_kernel` target: `qemu-system-aarch64 -M virt -cpu cortex-a57 -serial stdio`
- Adds `generate_hello_ll` target that runs `oberonc --emit-llvm tests/Hello.Mod`

### Usage
```bash
cmake -B build_apple -DKERNEL_ARCH=apple-aarch64
cmake --build build_apple --target kernel
cmake --build build_apple --target run_kernel   # prints "Hello, World!" via QEMU
```

---

## Part 3 — Project Oberon Kernel Image (In Progress)

### Goal
Compile all 14 Project Oberon source modules from `project-oberon/` into a single bare-metal AArch64 ELF image using the apple-aarch64 toolchain.

### Source Modules (excluding Demo files)

| Module | Dependencies |
|---|---|
| `Kernel` | SYSTEM |
| `Display` | SYSTEM |
| `Input` | SYSTEM |
| `FileDir` | SYSTEM, Kernel |
| `Files` | SYSTEM, Kernel, FileDir |
| `Modules` | SYSTEM, Files |
| `Fonts` | SYSTEM, Files |
| `Viewers` | Display |
| `Texts` | Files, Fonts |
| `Oberon` | SYSTEM, Kernel, Files, Modules, Input, Display, Viewers, Fonts, Texts |
| `MenuViewers` | Input, Display, Viewers, Oberon |
| `TextFrames` | Modules, Input, Display, Viewers, Fonts, Texts, Oberon, MenuViewers |
| `Edit` | Files, Fonts, Texts, Display, Viewers, Oberon, MenuViewers, TextFrames |
| `System` | Everything above |

**Topological compilation order:**
Kernel → Display → Input → FileDir → Files → Modules → Fonts → Viewers → Texts → Oberon → MenuViewers → TextFrames → Edit → System

### Compatibility Fixes Required

**`src/codegen.cpp` — `registerBuiltins()`**

Project Oberon uses Oberon-1/2 type names not present in Oberon-07:
- `LONGINT` — already registered as alias for `INTEGER` ✓
- `LONGREAL` — needs adding as alias for `REAL`

```cpp
typeTable_["LONGREAL"] = make(TypeKind::Real, "LONGREAL");
```

### Module Initialization Strategy

Since this is a static link (no runtime module loader), `oberon_main()` cannot be auto-generated from a single top-level module. Instead:

1. All 14 modules compiled with `--emit-llvm --init-only` → each produces `ModuleName_init()`
2. A hand-written C entry point (`runtime/oberon_kernel_entry.c`) provides `oberon_main()` that calls all inits in topological order

**`runtime/oberon_kernel_entry.c`:**
```c
void Kernel_init(void);
void Display_init(void);
void Input_init(void);
void FileDir_init(void);
void Files_init(void);
void Modules_init(void);
void Fonts_init(void);
void Viewers_init(void);
void Texts_init(void);
void Oberon_init(void);
void MenuViewers_init(void);
void TextFrames_init(void);
void Edit_init(void);
void System_init(void);

void oberon_main(void) {
    Kernel_init();
    Display_init();
    Input_init();
    FileDir_init();
    Files_init();
    Modules_init();
    Fonts_init();
    Viewers_init();
    Texts_init();
    Oberon_init();
    MenuViewers_init();
    TextFrames_init();
    Edit_init();
    System_init();
}
```

### CMake Target: `kernel_oberon`

Added inside the `apple-aarch64` block in `CMakeLists.txt`, after the existing `kernel` / `run_kernel` targets.

For each of the 14 modules, two custom commands:
1. `oberonc --emit-llvm --init-only project-oberon/Mod.Mod -o build_apple/Mod.ll`
2. `llc --march=aarch64 --filetype=obj build_apple/Mod.ll -o build_apple/Mod.o`

Plus:
- Compile `runtime/oberon_kernel_entry.c` → `oberon_kernel_entry.o` with `clang --target=aarch64-none-elf`
- Link: `boot.o` + `oberon_kernel_entry.o` + all 14 `.o` files + `liboberon_runtime_bare.a` → `kernel_oberon.elf`

### Verification

```bash
cmake -B build_apple -DKERNEL_ARCH=apple-aarch64
cmake --build build_apple --target kernel_oberon -j$(nproc)

file build_apple/kernel_oberon.elf
# ELF 64-bit LSB executable, ARM aarch64

qemu-system-aarch64 -M virt -cpu cortex-a57 \
    -kernel build_apple/kernel_oberon.elf \
    -serial stdio -display none -no-reboot
```

### Known Limitations (First Pass)

- **Hardware addresses**: Project Oberon modules target RISC hardware registers (e.g., `base = 0E7F00H` for VGA framebuffer, `msAdr = -40` for mouse). These will compile but produce incorrect runtime behavior on AArch64/QEMU. HAL adaptation is a follow-on task.
- **SYSTEM intrinsics**: `SYSTEM.PUT`/`SYSTEM.GET`/`SYSTEM.ADR` etc. compile via the existing SYSTEM support in `oberonc`. Correctness on AArch64 depends on the specific intrinsic.
- **Iterative**: Additional parse/codegen errors may surface during implementation and will be fixed as encountered.
