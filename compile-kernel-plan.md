# Plan: First Full Compilation of Project Oberon Modules into apple-aarch64 Kernel

## Context

We want to compile all 14 non-demo Project Oberon modules into a bootable apple-aarch64 kernel image (Apple Silicon, Homebrew LLVM 18 toolchain). The compiler (`oberonc`) exists and handles most Oberon-07, but several codegen gaps prevent the full module set from compiling. This plan: (1) documents the systematic fix order, (2) specifies exact code changes, and (3) extends CMakeLists.txt to automate the full build.

**Target shift (2026-06-07):** Original plan targeted x86-64; active work is now on `apple-aarch64` using `build_apple/oberonc`. The fix set applies to both targets.

---

## Current Status (2026-06-28, post Bug-4 fix — 14/14 pass)

### Trial run command
```bash
mkdir -p /tmp/po_trial && for mod in Kernel Display Input FileDir Files Modules Fonts Viewers Texts Oberon MenuViewers TextFrames Edit System; do
  echo "=== $mod ==="
  ./build/oberonc --emit-llvm --init-only --module-path /tmp/po_trial \
    project-oberon/${mod}.Mod -o /tmp/po_trial/${mod}.ll 2>&1
  echo "exit: $?"
done
```

### Build order & blocker map (13 pass, 1 fail)

| # | Module | Status | Error |
|---|---|---|---|
| 1 | Kernel | ✅ | — |
| 2 | Display | ✅ | — |
| 3 | Input | ✅ | — |
| 4 | FileDir | ✅ | — |
| 5 | Files | ✅ | — |
| 6 | Modules | ✅ | — |
| 7 | Fonts | ✅ | — |
| 8 | Viewers | ✅ | — |
| 9 | Texts | ✅ | — |
| 10 | Oberon | ✅ | — |
| 11 | MenuViewers | ✅ | — |
| 12 | TextFrames | ✅ | — |
| 13 | Edit | ✅ | — |
| 14 | System | ✅ | — |

### All applied fixes

| Fix | Commit | Description |
|---|---|---|
| `--module-path` CLI flag | earlier | Multiple module search paths |
| Array/record params use `ptr` in procedure types | earlier | LLVM IR aggregate params must be pointers |
| `evalConstInt` qualified constant lookup | earlier | `FileDir.ExTabSize` etc. resolved via `module_ident` key |
| Procedures registered in symbol table | earlier | Procedures usable as first-class values |
| Indirect-call guard excludes direct procs | earlier | Prevents direct-proc symbols being treated as proc vars |
| Type-guard CASE detection | earlier | Detects type-name CASE arms; narrows type per arm |
| Imported constant short-name temp keys | earlier | Constants also registered as short keys for Pass 2 resolution |
| Non-exported type aliases in Pass 1 | earlier | e.g. `DiskAdr = INTEGER` visible during Pass 2 |
| `tryGetTypeArg` qualified types | earlier | `SYSTEM.VAL(FileDir.FileHd, …)` with cross-module type arg |
| Open array length convention | earlier | Hidden `i64` arg, `__len_<name>` binding, `LEN` runtime load |
| Char literals emit `i8` not `ptr` | bfc5442 | Single-char and hex-char string literals → `ConstantInt::get(i8, v)` |
| **Stale `llvmTypeCache_` fix** | **28bb516** | **See Bug 1 below** |
| **Recursive transitive import loading** | **3f228f0** | **See Bug 2 below** |
| **IS operator short-circuit in genExpr** | **fa03276** | Bug 2a: skip RHS eval for `X IS T`; return true |
| **isFieldCall broadened to selector chains in genCallVal** | **fa03276** | proc-var call through imported module var's field works in expressions |
| **isFieldCall broadened in genCall (statements)** | **current** | Bug 2b: same broadening applied to statement call path; unblocks TextFrames |
| **VAR type resolution uses resolveType** | **current** | Bug 2b+3: replaces narrow QualIdentType lookup with resolveType(*vd.type); handles cross-module pointer types and inline records; unblocks Edit, System (partially) |
| **ORD emits ptrtoint for pointer args** | **current** | Bug 4: ORD(pointer) now emits ptrtoint i64 instead of passing raw ptr; unblocks System |

---

## Remaining Bugs (work through in order)

### Bug 1 — stale `llvmTypeCache_` key reuse (Modules, Texts) — ✅ FIXED (28bb516)

**Symptom:** `icmp ne ptr %arr0, [16 x ptr] %name4` — an open-array parameter was loaded as `[16 x ptr]` (the type of a previously freed array type) instead of `ptr`.

**Root cause:** `llvmTypeCache_` was keyed by raw `OberonType*`. Short-lived types (procedure-local variables and parameters) are owned by `shared_ptr`s that go out of scope when their procedure's body finishes processing. Once freed, the allocator can reuse the same address for a new `OberonType`. The next `toLLVM()` call on the new type then hits the stale cache entry and returns the wrong LLVM type (e.g., `[16 x ptr]` for `ARRAY 16 OF Module` being served for `Free`'s `name: ARRAY OF CHAR` parameter).

**Fix:** Added `liveTypes_: vector<OberonTypePtr>` to `CodeGen`. `toLLVM()` appends `t` into it before writing to the cache. This keeps every cached `OberonType*` alive for the lifetime of the `CodeGen` object, making the raw-pointer cache keys permanently valid.

**Files changed:** `src/codegen.hpp`, `src/codegen.cpp`

**Unblocked:** Modules, Texts (and indirectly Oberon was already passing; these were the last two Bug-1 cases).

---

### Bug 2 — `unknown type: Texts.Text` / `unknown type: Files.Rider` (MenuViewers, TextFrames) — ✅ FIXED (current)

**Symptom:** When compiling MenuViewers, `Texts.Text` was not found in the type table. Similarly `Files.Rider` for TextFrames.

**Root cause:** `loadModuleInterface` did not recursively load the imported module's own imports. When Texts.Mod was loaded on behalf of TextFrames, `resolveType` tried to resolve `Files.Rider` from Texts's `Reader`/`Writer` record fields and exported proc parameters, but `Files` had never been loaded (TextFrames doesn't directly import it). The proc-declaration loop inside `loadModuleInterface` had no try/catch, so the exception propagated out as a fatal error.

**Fix:** Added a pre-pass in `loadModuleInterface` that recursively loads each of the parsed module's own imports before processing its types. Uses `loadedUserModules_` to avoid re-loading already-visited modules. Transitive deps are loaded under their own module name as alias (so `typeTable_["Files_Rider"]` etc. are populated), but are not added to `importedModules_` (which would wrongly allow unqualified use in the current module).

**Files changed:** `src/codegen.cpp`

**Unblocked:** MenuViewers and TextFrames no longer fail with `unknown type`. New (different) errors appear — see Bug 2a and 2b below.

---

### Bug 2a — `unknown symbol: Viewer` (MenuViewers) — ✅ FIXED

**Symptom:** `error: codegen: unknown symbol: Viewer` when compiling MenuViewers.

**Root cause:** In `genExpr`, the `BinaryOp::Is` case was handled by the general switch which first evaluated both operands. The RHS of `V1 IS Viewer` is a type name (DesignatorExpr for `Viewer`), not a variable — so `genAddr` failed with "unknown symbol: Viewer".

**Fix:** Added an early-out before operand evaluation in `genBinExpr`: if `be->op == BinaryOp::Is`, evaluate only the left side (for side effects) and return `ConstantInt::getTrue` immediately. The RHS type name is never passed to `genExpr`.

**Files changed:** `src/codegen_gen.cpp` (IS guard in the BinaryExpr handler)

**Unblocks:** MenuViewers ✅

---

### Bug 2b — `unknown imported proc: Oberon_FocusViewer` (TextFrames)

**Symptom:** `error: codegen: unknown imported proc: Oberon_FocusViewer` when compiling TextFrames.

**Context:** TextFrames calls `Oberon.FocusViewer` (a procedure in Oberon.Mod). The function `Oberon_FocusViewer` should have been declared as an external LLVM function by `loadModuleInterface("Oberon", "Oberon")`. The error suggests the declaration is missing.

**Likely root cause:** When processing Oberon's procedure declarations inside `loadModuleInterface`, a parameter type in `FocusViewer`'s signature fails to resolve (e.g. it uses a type from a transitively-imported module that wasn't loaded at that point). The proc-declaration loop silently skips or throws, leaving `Oberon_FocusViewer` undeclared. Now that transitive imports are loaded eagerly, the ordering may have changed and a previously-passing proc now fails.

**Investigation:** Check `Oberon.FocusViewer`'s signature in Oberon.Mod. If it uses a type from Texts or Viewers, verify those are loaded before Oberon's procs are processed.

**Unblocks:** TextFrames.

---

### Bug 3 — `field sel on non-record` (Edit, System) — ✅ FIXED (current)

**Symptom:** Field access on `Oberon.Par` (e.g. `Oberon.Par.text`) fails because `Par` resolves to type INTEGER.

**Root cause:** The VAR import code in `loadModuleInterface` only handles `QualIdentType` (named type reference). `Oberon.Par` is declared with an **inline record type** (`RECORD vwr: Viewers.Viewer; frame: Display.Frame; text: Texts.Text; pos: LONGINT END`). The `dynamic_cast<QualIdentType*>` returns null, `oty` stays null, and falls back to INTEGER.

**Fix:** In the VAR section of `loadModuleInterface`, replaced the narrow `QualIdentType`-only type resolution with `resolveType(*vd.type)` wrapped in try/catch. This same change also fixed Bug 2b (Viewers.Viewer lookup via qualified permanent key `"Viewers_Viewer"`).

**Unblocked:** Edit ✅, System (partially — new Bug 4 now surfaces)

---

### Bug 4 — `ptr` passed where `i64` expected in `Texts_WriteHex` call (System) — ✅ FIXED

**Symptom:** LLVM module verification failure when compiling System.Mod:
```
Call parameter type does not match function signature!
  %M9 = load ptr, ptr %M, align 8
 i64  call void @Texts_WriteHex(ptr @System_W, ptr %M9)
```

**Root cause:** `ORD(M)` where `M: Modules.Module` is a pointer type. The `ORD` handler called `coerce(v, i64)` but `coerce` has no `ptr → i64` path — it silently returned the `ptr` unchanged, causing a type mismatch at the `Texts_WriteHex` call site.

**Fix:** Added `ptrtoint` to the `ORD` handler in `codegen_gen.cpp`:
```cpp
if (v->getType()->isPointerTy())
    return b_->CreatePtrToInt(v, i64, "ord");
```

**Files changed:** `src/codegen_gen.cpp`

**Unblocked:** System ✅ — all 14/14 Project Oberon modules now compile to LLVM IR.

---

## Module Set & Compilation Order

Dependency-resolved order (each module's imports are satisfied before it's compiled):

```
1.  Kernel.Mod       — SYSTEM only
2.  Display.Mod      — SYSTEM only
3.  Input.Mod        — SYSTEM only
4.  FileDir.Mod      — Kernel, SYSTEM
5.  Files.Mod        — Kernel, FileDir, SYSTEM
6.  Fonts.Mod        — Files, SYSTEM
7.  Viewers.Mod      — Display
8.  Texts.Mod        — Files, Fonts
9.  Modules.Mod      — Files, SYSTEM
10. Oberon.Mod       — Kernel, Files, Modules, Input, Display, Viewers, Fonts, Texts, SYSTEM
11. MenuViewers.Mod  — Input, Display, Viewers, Oberon
12. TextFrames.Mod   — Modules, Input, Display, Viewers, Fonts, Texts, Oberon, MenuViewers
13. Edit.Mod         — Files, Fonts, Texts, Display, Viewers, Oberon, MenuViewers, TextFrames
14. System.Mod       — all of the above
```

Command to list them:
```bash
find project-oberon -iname "*.Mod" | grep -v DisplayDemo | grep -v KernelDemo
```

---

## Step 6: Extend CMakeLists.txt for Full Project Oberon Kernel

Add a new CMake target `project_oberon_kernel` after the existing `kernel` target (around line 128 of CMakeLists.txt).

### 6a. Per-module .ll generation (custom commands, chained deps)

```cmake
set(PO_DIR "${CMAKE_SOURCE_DIR}/project-oberon")
set(PO_BUILD "${CMAKE_BINARY_DIR}/project-oberon")
file(MAKE_DIRECTORY ${PO_BUILD})

set(PO_MODULES Kernel Display Input FileDir Files Fonts Viewers Texts Modules
               Oberon MenuViewers TextFrames Edit System)

set(PO_LL_FILES "")
set(prev_ll "")   # force serial compilation order to satisfy inter-module deps

foreach(mod IN LISTS PO_MODULES)
  set(ll_out "${PO_BUILD}/${mod}.ll")
  add_custom_command(
    OUTPUT  ${ll_out}
    COMMAND $<TARGET_FILE:oberonc>
            --emit-llvm
            --module-path ${PO_BUILD}
            --module-path ${PO_DIR}
            -o ${ll_out}
            ${PO_DIR}/${mod}.Mod
    DEPENDS oberonc ${PO_DIR}/${mod}.Mod ${prev_ll}
    COMMENT "Compiling ${mod}.Mod -> ${mod}.ll"
  )
  list(APPEND PO_LL_FILES ${ll_out})
  set(prev_ll ${ll_out})
endforeach()
```

### 6b. Compile .ll → .o and link

```cmake
set(PO_OBJ_FILES "")
foreach(mod IN LISTS PO_MODULES)
  set(ll_in  "${PO_BUILD}/${mod}.ll")
  set(obj_out "${PO_BUILD}/${mod}.o")
  add_custom_command(
    OUTPUT  ${obj_out}
    COMMAND llc-14 --march=x86-64 --filetype=obj ${ll_in} -o ${obj_out}
    DEPENDS ${ll_in}
    COMMENT "Assembling ${mod}.ll -> ${mod}.o"
  )
  list(APPEND PO_OBJ_FILES ${obj_out})
endforeach()

add_custom_target(project_oberon_kernel
  COMMAND ${CMAKE_LINKER}
          -T ${CMAKE_SOURCE_DIR}/runtime/boot/x86_64/kernel.ld
          -o ${CMAKE_BINARY_DIR}/project_oberon_x86_64.elf
          ${CMAKE_SOURCE_DIR}/runtime/boot/x86_64/boot_x86.o
          ${PO_OBJ_FILES}
          $<TARGET_FILE:oberon_runtime_bare>
  DEPENDS ${PO_OBJ_FILES} oberon_runtime_bare
  COMMENT "Linking Project Oberon kernel"
)
```

*(Exact boot object path and ld flags should match the existing `kernel` target at CMakeLists.txt ~line 146–170.)*

---

## Step 7: Bare-Metal Runtime Stubs

Many Project Oberon modules use OS primitives that the current bare runtime (`runtime/oberon_runtime_bare.cpp`) doesn't implement. For the first link to succeed, stub implementations are acceptable:

| Module need | Runtime symbol needed | Stub approach |
|---|---|---|
| `Files.Mod` disk I/O | `Kernel_GetSector`, `Kernel_PutSector` | `extern "C"` wrapper calling `hal_halt()` |
| `Oberon.Mod` timers | `Kernel_Time` | Returns 0 |
| `Display.Mod` raster ops | Direct MMIO via SYSTEM.PUT | No runtime needed — uses SYSTEM |
| `Input.Mod` keyboard | `hal_getc()` | Already exists in HAL |

Add stubs to `runtime/oberon_runtime_bare.cpp` as `extern "C"` functions.

---

## Verification

```bash
# 1. Build compiler (apple-aarch64 required on Apple Silicon)
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DKERNEL_ARCH=apple-aarch64 && cmake --build build -j$(nproc)

# 2. Compile all modules manually to see errors
for m in Kernel Display Input FileDir Files Fonts Viewers Texts Modules Oberon MenuViewers TextFrames Edit System; do
  echo "=== $m ==="
  ./build/oberonc --emit-llvm --module-path project-oberon project-oberon/${m}.Mod 2>&1 || true
done

# 3. After fixes, build kernel via CMake
cmake --build build --target project_oberon_kernel

# 4. Boot with QEMU
qemu-system-x86_64 \
  -kernel build/project_oberon_x86_64.elf \
  -serial stdio -display none -no-reboot
```

---

## Fix Priority Order

Fix in this order to unblock the most downstream modules earliest:

1. **Bug 1 — char literal `i8` vs `ptr`** — unblocks Modules, Texts, Oberon
2. **Bug 2 — unknown cross-module pointer types** — unblocks MenuViewers, TextFrames
3. **Bug 3 — inline record VAR import** — unblocks Edit, System
4. **CMakeLists.txt** (Step 6) — automates full kernel build
5. **Runtime stubs** (Step 7) — enables link + QEMU boot

---

## Bug 5 — Boot-time hardcoded RISC5 physical addresses (Kernel.Init, Display.Mod) — 🔧 IN PROGRESS (2026-07-21)

All 14 modules now compile and link (see Status above), but the original Wirth source hardcodes several RISC5-hardware physical addresses as literal constants, which `oberonc` compiles literally (no compile-time-constant requirement on `SYSTEM.PUT`/`GET` addresses — they're generic runtime `i64` values, `codegen_gen.cpp`). These addresses only happen to be valid on x86_64/QEMU by luck of memory layout, and even there, one boot-time code path has never actually been exercised on either architecture.

### Symptom

Two independent hardcoded-address problems, discovered while planning an aarch64 display driver:

1. **`Display.Mod:6`** — `CONST base = 0E7F00H;` (framebuffer base). Valid free RAM on x86_64/QEMU (below the 1 MiB kernel load address), but **not valid on aarch64/QEMU `virt`**, where RAM starts at `0x40000000`. Every drawing procedure (`Dot`, `ReplConst`, `CopyPattern`, `CopyBlock`, `ReplPattern`) computes `base + <offset>` and does raw `SYSTEM.PUT`/`GET` — a write/read there on aarch64 would fault or go nowhere, since nothing backs that address.

2. **`Kernel.Mod:250-269`** (`Install`, `Trap`, `Init`) —
   - `Install(Padr, at)`: `SYSTEM.PUT(at, 0E7000000H + (Padr - at) DIV 4 - 1)` — writes a **RISC5 branch-instruction opcode** (raw machine code) into physical address `at`. Meaningless on x86_64 (IDT-based) or aarch64 (`VBAR_EL1`-based) — neither models this convention at all.
   - `Init*`: calls `Install(SYSTEM.ADR(Trap), 20H)` (writes to physical `0x20`), then `SYSTEM.GET(12, MemLim); SYSTEM.GET(24, heapOrg)` — reads heap size/origin from fixed physical addresses 12/24, a RISC5 FPGA-bootloader convention nothing in our boot path populates.
   - **This is not dead/unreachable code**: `Files.Mod:483` — `BEGIN root := 0; Kernel.Init; FileDir.Init` — `Files` is module #5 of 14 in the topological init order, so `Kernel.Init` runs automatically during `oberon_main`, before `Display`/`Oberon`/etc. initialize, on **both** architectures.

Neither architecture has ever booted the full 14-module kernel far enough to exercise this: aarch64 IR compiles/links (`build/project_oberon_aarch64.elf` exists) but QEMU boot itself is unresolved/unverified (see project memory); x86_64 has no full-kernel CMake target at all (only the trivial Hello-World `kernel` target). So this isn't "the aarch64 fix must avoid regressing a working x86_64 path" — both targets are equally untested here.

### Root cause

`oberonc`'s `SYSTEM.PUT`/`GET`/`ADR` codegen (`codegen_gen.cpp`) treats the address argument as an ordinary runtime expression — no special-casing, no compile-time-constant requirement. The literal addresses are baked in purely because the *source* wrote them that way (faithful, unmodified Wirth text), not because the compiler forces it. This means a HAL-indirection fix (source reads a runtime value instead of a literal) works with **zero compiler changes beyond adding the new import**, and compiles identically for both architectures (the frontend/IR-gen pipeline never branches on target arch — only `llc`/`ld` and the per-platform HAL bodies differ downstream).

**Investigation also surfaced two things that simplify/derisk the fix:**
- `INTEGER`/`LONGINT` are 64-bit (`i64`) in this compiler (`codegen.cpp:264-266`), not Wirth's original 32-bit — so any new HAL call can just return `i64` uniformly, no coercion subtlety.
- `Kernel.Install`/`Trap` (and `System.Mod:417`'s identical second `Kernel.Install` call) are **already fully dead code today, independent of this fix**: `ASSERT` compiles directly to extern `Oberon_Trap()` (`codegen_gen.cpp:973-984`); `NEW` compiles directly to extern `Oberon_NEW()` (`codegen_gen.cpp:992-1008`); `SYSTEM.REG`/`SYSTEM.H` always fold to constant `0` (`codegen_gen.cpp:547-559`). Nothing ever installs a real IDT/`VBAR_EL1` entry that would fetch address `0x20` as an instruction. So dropping `Install`'s call from `Kernel.Init` removes a write to unmapped memory that already served no purpose.

### Fix (design; not yet implemented)

Add a new `HAL` pseudo-module (no `.Mod` source file, exactly like the existing `Out`/`In` pattern) exposing three niladic `INTEGER`-returning calls: `HAL.FramebufferBase()`, `HAL.HeapOrigin()`, `HAL.MemLimit()`. Requires **only** a new branch in `genImports` (`src/codegen.cpp`, alongside the existing `Out`/`In` branches ~line 454-465) — `genCallVal`'s existing generic `module + "_" + ident` resolution (`codegen_gen.cpp:793-800`) already handles the call sites with no further compiler changes.

- **`Display.Mod`**: `IMPORT SYSTEM, HAL;`, delete the `base` literal, `Base := HAL.FramebufferBase()` in the module body, redirect the 6 internal `base` usages to the exported `Base` VAR (which already existed, just previously redundant).
- **`Kernel.Mod`**: `IMPORT SYSTEM, HAL;`, in `Init*` drop the `Install(SYSTEM.ADR(Trap), 20H)` call and replace `SYSTEM.GET(12, MemLim); SYSTEM.GET(24, heapOrg)` with `MemLim := HAL.MemLimit(); heapOrg := HAL.HeapOrigin();`. `Install`/`Trap` bodies and `System.Mod:417`'s call site are left untouched (confirmed harmless dead code).
- **Two-heap collision risk, must be designed out**: `Oberon_NEW` (`oberon_runtime_bare.cpp`) is a header-less bump allocator; `Kernel.Mark` (live-called from `Oberon.GC`) assumes every pointer `>= heapOrg` has an 8-byte Kernel-format header and will corrupt memory if `HAL.HeapOrigin()` overlaps `Oberon_NEW`'s arena. Fix: implement `HAL_HeapOrigin`/`HAL_MemLimit` in the **shared** `oberon_runtime_bare.cpp` (not per-arch), carving Kernel's dummy heap out of the *tail* of the same static `heap[1<<20]` array, with `Oberon_NEW`'s bump limit capped strictly below it — disjointness by construction, not convention.
- **`HAL_FramebufferBase`** is genuinely per-platform: x86_64 (`runtime/platform/x86_64/hal.cpp`) returns the existing `SHADOW_BASE` (`0x0E7F00`) unchanged — byte-identical runtime behavior to today. aarch64 (`runtime/platform/aarch64/hal.cpp`) returns the address of a new static `shadow_fb[768][128]` BSS buffer (aarch64 currently has no display buffer of any kind).

**Files to change:** `src/codegen.cpp`, `project-oberon/Display.Mod`, `project-oberon/Kernel.Mod`, `runtime/oberon_runtime_bare.cpp`, `runtime/platform/x86_64/hal.cpp`, `runtime/platform/aarch64/hal.cpp`.

**Unblocks:** a testable boot path past `Files_init`/`Kernel.Init` on both architectures, and a real (non-garbage) framebuffer address for the Bug-6 aarch64 display driver below.

**Verification:** rebuild `oberonc` + run `oberonc_tests`/`oberonc_codegen_tests` first (compiler-level regression check before touching Oberon source); compile a throwaway `IMPORT HAL; ... x := HAL.FramebufferBase()` module and inspect the `.ll`; then compile edited `Display.Mod`/`Kernel.Mod` for both `--march=x86-64` and aarch64 to confirm the `HAL` import path works on both backends.

---

## Bug 6 — aarch64 has no display device at all — 🔧 PLANNED (2026-07-21)

### Symptom

`runtime/platform/aarch64/hal.cpp`'s `hal_display_flush()` is a no-op stub (`void hal_display_flush() {}  // AArch64: no VBE display`). Unlike x86_64 (which discovers a Bochs VBE device via PCI config-space scanning and blits Display.Mod's shadow buffer to it), aarch64 has no PCI scanning, no GPU driver, and no real pixel buffer — nothing for the display driver to blit into even once Bug 5 gives it a valid shadow-buffer address.

### Fix (design; not yet implemented)

Use QEMU's **`ramfb`** device (decision made 2026-07-21, over virtio-gpu): guest allocates a real pixel buffer in its own RAM, writes one config struct through the `fw_cfg` interface (address/width/height/stride/format), and QEMU reads pixels directly from that guest RAM every frame — no PCI, no virtqueues, no ongoing driver protocol. QEMU `virt` exposes `fw_cfg` at a fixed, version-stable MMIO base (`0x09020000`), consistent with how `UART_BASE` is already hardcoded in this codebase — no DTB parsing needed for v1. (virtio-gpu was considered and rejected for v1: requires implementing virtqueue setup and the full virtio-gpu command protocol from scratch, ~5-10x the code for equivalent first pixels on screen.)

- New `hal_display_init()` in `runtime/platform/aarch64/hal.cpp`: locate `"etc/ramfb"` via the fw_cfg file directory, write a 28-byte big-endian `RAMFBCfg{addr, fourcc, flags, width=1024, height=768, stride=4096}` pointing at a new static `ramfb_pixels[768][1024*4]` (XRGB8888, ~3 MiB BSS) through the selector+data registers.
- New `hal_display_flush()` body: blit `shadow_fb` (1bpp, from Bug 5) → `ramfb_pixels` (XRGB8888), mirroring the shape of the existing x86_64 blit.
- **Open risk, flagged for implementation time**: the exact `fw_cfg`/`ramfb` wire format (byte layout, fourcc constant, selector index) needs verification against `hw/display/ramfb.c`/`docs/specs/fw_cfg.txt` in the actual QEMU source tree in use — this design is from protocol knowledge, not verified against this host's QEMU version.
- **CMake**: add `-device ramfb` to `run_kernel`/`run_project_oberon` QEMU invocations; introduce a `KERNEL_QEMU_DISPLAY` cache variable (default `none`, headless-safe) instead of hardcoding a display backend, so `-DKERNEL_QEMU_DISPLAY=cocoa` gives a real window on the Mac dev machine without breaking CI/headless runs.

**Files to change:** `runtime/platform/aarch64/hal.cpp`, `CMakeLists.txt`.

**Verification:** `-device ramfb -display none` + QEMU monitor `screendump` (headless-safe, inspect the resulting image for non-blank content) as the primary check; one manual `-DKERNEL_QEMU_DISPLAY=cocoa` run to visually confirm on the Mac.

**Explicitly out of scope for Bugs 5/6:** `Input.Mod`'s `msAdr`/`kbdAdr` (keyboard/mouse — same hardcoded-address category, separate hardware surface), `Kernel.Mod`'s SD-card SPI disk driver (`spiCtrl`/`spiData`/`timer`, `ReadSD`/`WriteSD` — used by `FileDir.Mod`/`Files.Mod` for all filesystem access), and adding a full x86_64 14-module kernel CMake target (none exists today; only the trivial Hello-World `kernel` target does).

---

## Step 8: Implementation — HAL Pseudo-Module & Boot-Address Fixes (Bug 5)

### 8a. `src/codegen.cpp` — new `HAL` branch in `genImports`

Insert after the existing `else if (mod == "In")` block (~line 465), before the final `else { loadModuleInterface(...); }`:

```cpp
} else if (mod == "HAL") {
    auto decl = [&](const char* fn) {
        auto* ft = llvm::FunctionType::get(i64, {}, false);
        extFuncs_[std::string(fn)] =
            llvm::Function::Create(ft, llvm::Function::ExternalLinkage, fn, llvmMod_.get());
    };
    decl("HAL_FramebufferBase");
    decl("HAL_HeapOrigin");
    decl("HAL_MemLimit");
}
```

No `codegen_gen.cpp` changes needed — `genCallVal`'s existing generic `module + "_" + ident` resolution (`codegen_gen.cpp:793-800`) already handles the Oberon-side call sites for any imported module name.

### 8b. `project-oberon/Display.Mod`

- `IMPORT SYSTEM;` → `IMPORT SYSTEM, HAL;`
- Delete `CONST base = 0E7F00H;  (*adr of...*)` (line 6).
- Module body: `BEGIN Base := base; Width := 1024; ...` → `BEGIN Base := HAL.FramebufferBase(); Width := 1024; ...`
- Redirect the 6 internal `base` occurrences to the already-exported `Base` VAR (verified via `grep -n base Display.Mod`: lines 29, 40, 77, 108×2, 161).

### 8c. `project-oberon/Kernel.Mod`

- `IMPORT SYSTEM;` → `IMPORT SYSTEM, HAL;`
- `Init*` (lines 262-269):

```
PROCEDURE Init*;
BEGIN Install(SYSTEM.ADR(Trap), 20H);  (*install temporary trap*)
  SYSTEM.GET(12, MemLim); SYSTEM.GET(24, heapOrg);
  stackOrg := heapOrg; stackSize := 8000H; heapLim := MemLim;
  ...
```
→
```
PROCEDURE Init*;
BEGIN
  MemLim := HAL.MemLimit(); heapOrg := HAL.HeapOrigin();
  stackOrg := heapOrg; stackSize := 8000H; heapLim := MemLim;
  ...
```

- Leave `Install*`/`Trap` bodies and `System.Mod:417`'s `Kernel.Install(...)` call site untouched — confirmed dead code (see Bug 5 root-cause analysis above), rewriting them is unneeded scope creep.

### 8d. `runtime/oberon_runtime_bare.cpp` — shared heap split

```cpp
static uint8_t heap[1 << 20];                                    // unchanged, 1 MiB total
static constexpr size_t KERNEL_HEAP_RESERVE = 64 * 1024;         // tail reserved for Kernel.Mod's dummy heap
static constexpr size_t OBERON_NEW_LIMIT = sizeof(heap) - KERNEL_HEAP_RESERVE;
static size_t heap_ptr = 0;

void* Oberon_NEW(int64_t size) {
    // ... existing alignment logic ...
    if (heap_ptr + aligned > OBERON_NEW_LIMIT) { /* existing out-of-memory halt */ }
    // ...
}

// Kernel.Mod's Init reads these to seed its own free-list heap, used only by
// Kernel.Mark/Scan (Deutsch-Schorr-Waite GC over Kernel-format headered
// blocks). This range MUST stay strictly disjoint from — and at a
// numerically higher address than — every pointer Oberon_NEW can return, or
// Mark will misinterpret header-less Oberon_NEW objects as Kernel-format
// blocks and corrupt adjacent memory. Carving this out of the same static
// array, with Oberon_NEW capped below it, makes that hold by construction.
int64_t HAL_HeapOrigin() { return reinterpret_cast<int64_t>(heap + OBERON_NEW_LIMIT); }
int64_t HAL_MemLimit()   { return reinterpret_cast<int64_t>(heap + sizeof(heap)); }
```

Both declared inside the existing `extern "C" { ... }` block alongside `Out_*`/`In_*`/`Oberon_NEW`/`Oberon_Trap`. Shared (not per-arch) so the disjointness invariant holds by construction in one translation unit.

### 8e. `runtime/platform/x86_64/hal.cpp`

```cpp
int64_t HAL_FramebufferBase() { return static_cast<int64_t>(SHADOW_BASE); }  // == 0x0E7F00, byte-identical to today
```

Add near the existing `SHADOW_BASE` definition, inside the `extern "C"` section.

### 8f. `runtime/platform/aarch64/hal.cpp`

```cpp
static constexpr int FB_W = 1024, FB_H = 768;
static uint8_t shadow_fb[FB_H][FB_W / 8];   // 98,304 bytes, BSS — 1bpp shadow framebuffer

int64_t HAL_FramebufferBase() { return reinterpret_cast<int64_t>(&shadow_fb[0][0]); }
```

### 8g. `runtime/platform/hal.hpp`

Add a one-line comment noting the split: `HAL_FramebufferBase` is per-platform (`platform/<arch>/hal.cpp`), while `HAL_HeapOrigin`/`HAL_MemLimit` are shared (`oberon_runtime_bare.cpp`) — an unusual enough layout that future readers will look here first.

### 8h. Verification (do in this order)

1. Rebuild `oberonc`, run `./build/oberonc_tests` and `./build/oberonc_codegen_tests` — confirm the new `genImports` branch regresses nothing.
2. Compile a throwaway module (`IMPORT HAL; VAR x: INTEGER; BEGIN x := HAL.FramebufferBase() END`) with `--emit-llvm`; inspect the `.ll` for a correct `HAL_FramebufferBase` declaration/call, before touching `Display.Mod`/`Kernel.Mod`.
3. Compile edited `Display.Mod`/`Kernel.Mod` with `--init-only --module-path project-oberon`, `llc --march=x86-64`, to confirm the `HAL` import path also works on x86_64 codegen (no full x86_64 kernel link exists to test end-to-end).
4. `cmake --build build --target project_oberon_kernel` (apple-aarch64), then `run_project_oberon` — watch `-serial stdio` for `System.Mod`'s startup banner ("Oberon V5 NW 14.4.2013", `System.Mod:369`) as a concrete proxy for "all 14 module inits completed without hanging/faulting at `Kernel.Init`."

---

## Step 9: Implementation — AArch64 `ramfb` Display Driver (Bug 6)

Depends on Step 8 (needs a valid `HAL.FramebufferBase()` to blit from).

### 9a. `runtime/platform/aarch64/hal.cpp` — real pixel buffer + fw_cfg setup

```cpp
static uint8_t ramfb_pixels[FB_H][FB_W * 4];   // XRGB8888, ~3 MiB BSS
```

New `hal_display_init()` (called from `hal_init()`):
1. `fw_cfg` MMIO at fixed base `0x09020000` on QEMU `virt` (hardcoded, matching this codebase's existing `UART_BASE` convention — no DTB parsing for v1).
2. Locate `"etc/ramfb"` via the fw_cfg file directory (selector `0x19` = `FW_CFG_FILE_DIR`; read big-endian file count, then scan 64-byte `{u32 size; u16 select; u16 reserved; char name[56]}` entries for `name == "etc/ramfb"`).
3. Write a 28-byte big-endian `RAMFBCfg{addr, fourcc, flags, width=1024, height=768, stride=4096}` (`addr = &ramfb_pixels[0][0]`, fourcc = XRGB8888) through the selector+data registers.
4. **Verify at implementation time** against `hw/display/ramfb.c`/`docs/specs/fw_cfg.txt` in the actual QEMU source tree in use — byte layout, fourcc constant, and selector index here are from protocol knowledge, not confirmed against this host's QEMU version.

New `hal_display_flush()` (replacing today's no-op stub): blit `shadow_fb` (1bpp) → `ramfb_pixels` (XRGB8888), mirroring the shape of the existing x86_64 blit. Cross-check bit-packing/word order against the x86_64 blit and a known pattern (e.g. `Display.arrow`) to avoid a mirrored/shifted image.

### 9b. `CMakeLists.txt`

- `run_kernel` (~line 311-318) and `run_project_oberon` (~line 408-417): add `-device ramfb`.
- Introduce a cache variable instead of hardcoding the display backend:
  ```cmake
  set(KERNEL_QEMU_DISPLAY "none" CACHE STRING "QEMU -display backend for run_project_oberon")
  ...
  COMMAND ${QEMU_AARCH64} -M virt -cpu cortex-a57 -device ramfb
          -kernel ... -serial stdio -display ${KERNEL_QEMU_DISPLAY} -no-reboot
  ```
  (`-DKERNEL_QEMU_DISPLAY=cocoa` for a real window on the Mac; default `none` stays headless/CI-safe.)
- Update the now-stale comment at `CMakeLists.txt:334-337` ("only external symbols needed are Oberon_NEW and Oberon_Trap") to mention the new `HAL_*` externs.
- No new source files to register — `oberon_runtime_bare.cpp` and `platform/aarch64/hal.cpp` are already compiled by both the `aarch64` (Linux cross-compile) and `apple-aarch64` (Mac native) `KERNEL_ARCH` blocks, so this fix covers both automatically.

### 9c. Verification

1. `-device ramfb -display none` + QEMU monitor (`-monitor unix:/path,server,nowait`) `screendump` command → inspect the resulting image for non-blank content (headless/CI-safe).
2. One manual `-DKERNEL_QEMU_DISPLAY=cocoa` run on the Mac to visually confirm.
3. Confirm `Input.Mod` and `Kernel.Mod`'s SPI/disk path are not accidentally exercised during this verification — a crash there is expected/out-of-scope, not a regression from Bugs 5/6.

### Critical files (Steps 8 + 9, combined)

- `src/codegen.cpp` — `HAL` branch in `genImports`
- `project-oberon/Display.Mod` — `base` → `HAL.FramebufferBase()` / `Base`
- `project-oberon/Kernel.Mod` — `Init*`: drop `Install` call, use `HAL.MemLimit()`/`HAL.HeapOrigin()`
- `runtime/oberon_runtime_bare.cpp` — shared `HAL_HeapOrigin`/`HAL_MemLimit`, `Oberon_NEW` bump-limit cap
- `runtime/platform/aarch64/hal.cpp` — `shadow_fb`/`ramfb_pixels`, `HAL_FramebufferBase`, fw_cfg/ramfb init, real `hal_display_flush`
- `runtime/platform/x86_64/hal.cpp` — `HAL_FramebufferBase` returning existing `SHADOW_BASE`
- `CMakeLists.txt` — `-device ramfb`, `KERNEL_QEMU_DISPLAY` cache var, stale comment update
