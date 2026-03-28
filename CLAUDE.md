# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Commands

```bash
# Configure (first time or after CMakeLists.txt changes)
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build everything (compiler + tests)
cmake --build build -j$(nproc)

# Run the compiler on an Oberon source file
./build/oberonc tests/Hello.Mod

# Run all tests
./build/oberonc_tests

# Run a specific test by name
./build/oberonc_tests "Lexer: nested comments are handled"

# Run a test tag group
./build/oberonc_tests "[lexer]"
./build/oberonc_tests "[parser]"

# Build with AddressSanitizer
cmake -B build -DENABLE_ASAN=ON && cmake --build build -j$(nproc)
```

Catch2 v3 is fetched automatically by CMake via `FetchContent` on first configure (requires internet access).

## Architecture

The project implements a **recursive-descent LL(1) parser** for the **Oberon-07** language (Niklaus Wirth, 2016 revision). The pipeline is: source text → Lexer → token stream → Parser → AST.

### Source files (`src/`)

| File | Role |
|---|---|
| `token.hpp` | `TokenKind` enum + `Token` struct + `SourceLoc`. `tokenKindName()` is defined in `lexer.cpp`. |
| `lexer.hpp/cpp` | Hand-written lexer. Produces one `Token` per `next()` call. Handles nested `(* ... *)` comments, decimal and hex integers, real literals with scale factors, string literals (`"..."` and `0DX`-style char constants). |
| `ast.hpp` | All AST node types. Uses `std::unique_ptr` for ownership, `std::variant<FieldSelector, IndexSelector, DerefSelector, TypeGuardSelector>` for selectors. |
| `parser.hpp/cpp` | Recursive descent parser. One `parse*()` method per grammar production. Maintains a single lookahead token (`cur_`). Throws `ParseError` on syntax errors. |
| `main.cpp` | CLI entry point: reads a file, runs Lexer+Parser, prints a summary. |

### Key design decisions

**qualident vs field selector ambiguity**: Per the Oberon-07 grammar, `r.x` is parsed as `qualident(module="r", ident="x")` — a two-part qualified name with no selectors. Whether `r` refers to an imported module or a record variable is resolved in semantic analysis, not during parsing. Only `p^.x` (pointer deref then field access) produces an unambiguous `FieldSelector`.

**Type guard selector `(T)`**: The grammar allows `selector = "(" qualident ")"` inside a designator. To keep LL(1) without extra lookahead, `(` is *not* consumed inside `parseDesignator`. The caller (`parseFactor` / `parseStatement`) handles `(` as `ActualParameters`. The semantic phase distinguishes type guards from procedure calls.

**`std::format` unavailability**: GCC 12 does not ship `<format>`. A variadic `fmt()` helper using `std::ostringstream` is defined at the top of `parser.cpp` instead.

### Grammar reference

The full Oberon-07 EBNF is in the Appendix of the official language report: `Oberon07.Report.pdf` (Niklaus Wirth, ETH Zurich). The key productions the parser implements are: `module`, `DeclarationSequence`, `ProcedureDeclaration`, `statement`, `expression`, `designator`, `type`.

## Bootable images (freestanding runtime)

The `runtime/` directory contains a bare-metal runtime that replaces `liboberon_runtime.a` for freestanding targets (no OS, no libc).

### Architecture

| Path | Role |
|---|---|
| `runtime/oberon_runtime_bare.cpp` | Implements `Out_*`, `In_*`, `Oberon_NEW` via `hal_*` calls; 1 MiB bump-allocator heap in BSS |
| `runtime/platform/hal.hpp` | HAL interface: `hal_init()`, `hal_putc()`, `hal_getc()`, `hal_halt()` |
| `runtime/platform/x86_64/hal.cpp` | x86-64 HAL: VGA text buffer (0xB8000) + COM1 UART (0x3F8, 38400 baud 8N1) |
| `runtime/platform/aarch64/hal.cpp` | AArch64 HAL: PL011 UART at 0x09000000 (QEMU `virt` machine, 38400 baud) |
| `runtime/boot/x86_64/boot.S` | Multiboot2 header, 32→64 mode switch, identity-maps first 1 GiB with 2 MiB huge pages, zeroes BSS |
| `runtime/boot/x86_64/kernel.ld` | Linker script: loads at 0x100000, `.multiboot2` first |
| `runtime/boot/aarch64/boot.S` | Drops from EL2→EL1, sets stack, zeroes BSS |
| `runtime/boot/aarch64/kernel.ld` | Linker script: loads at 0x40080000 |

Boot sequence in both architectures: `_start` → `hal_init()` → `oberon_main()` → `hal_halt()`.

### CMake targets

Select the target architecture at configure time with `-DKERNEL_ARCH=` (default: `x86_64`):

```bash
# x86-64 kernel (default)
cmake -B build -DKERNEL_ARCH=x86_64
cmake --build build --target kernel
# Output: build/kernel_x86_64.elf

# AArch64 kernel (requires aarch64-linux-gnu-{gcc,g++,ld})
cmake -B build -DKERNEL_ARCH=aarch64
cmake --build build --target kernel
# Output: build/kernel_aarch64.elf

# Override cross-compiler paths if needed
cmake -B build -DKERNEL_ARCH=aarch64 \
      -DAARCH64_CXX=/opt/cross/bin/aarch64-elf-g++ \
      -DAARCH64_LD=/opt/cross/bin/aarch64-elf-ld
```

The `kernel` target assembles `tests/Hello.ll` with `llc`/`llc-14`, links the boot stub and the bare runtime library for the selected architecture.

### Running with QEMU

```bash
# x86-64: serial output on stdio; Multiboot2 via GRUB/QEMU direct kernel boot
qemu-system-x86_64 -kernel build/kernel_x86_64.elf \
    -serial stdio -display none -no-reboot

# AArch64: QEMU virt machine; PL011 UART on stdio
qemu-system-aarch64 -M virt -cpu cortex-a57 \
    -kernel build/kernel_aarch64.elf \
    -serial stdio -display none -no-reboot
```

### Compile flags

| Flag | Reason |
|---|---|
| `-ffreestanding` | No implicit libc; disables built-ins that assume OS |
| `-mno-red-zone` | x86-64 only; interrupt handlers may clobber the red zone |
| `-O2` | Required for FPU-safe code generation in the bare runtime |

### Adding a new Oberon module to the kernel

1. Compile the `.Mod` file to LLVM IR: `./build/oberonc MyMod.Mod` → `MyMod.ll`
2. Assemble to object: `llc-14 --march=x86-64 --filetype=obj MyMod.ll -o MyMod.o` (or `llc --march=aarch64` for AArch64)
3. Link: replace `hello_x86.o` with `MyMod.o` in the `ld` invocation (or extend `CMakeLists.txt`)
4. Boot with QEMU as above.
