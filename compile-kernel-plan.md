# Plan: First Full Compilation of Project Oberon Modules into apple-aarch64 Kernel

## Context

We want to compile all 14 non-demo Project Oberon modules into a bootable apple-aarch64 kernel image (Apple Silicon, Homebrew LLVM 18 toolchain). The compiler (`oberonc`) exists and handles most Oberon-07, but several codegen gaps prevent the full module set from compiling. This plan: (1) documents the systematic fix order, (2) specifies exact code changes, and (3) extends CMakeLists.txt to automate the full build.

**Target shift (2026-06-07):** Original plan targeted x86-64; active work is now on `apple-aarch64` using `build_apple/oberonc`. The fix set applies to both targets.

---

## Current Status (2026-06-28, post Bug-2b/Bug-3 fix — 13/14 pass)

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
| 14 | **System** | ❌ | LLVM verify: `ptr` passed where `i64` expected in `Texts_WriteHex` call (Bug 4) |

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

### Bug 4 — `ptr` passed where `i64` expected in `Texts_WriteHex` call (System)

**Symptom:** LLVM module verification failure when compiling System.Mod:
```
Call parameter type does not match function signature!
  %M9 = load ptr, ptr %M, align 8
 i64  call void @Texts_WriteHex(ptr @System_W, ptr %M9)
```

**Context:** `Texts.WriteHex(VAR W: Writer; x: LONGINT)` — second param is `LONGINT` (i64). System.Mod calls it as `Texts.WriteHex(W, ORD(M))` and `Texts.WriteHex(W, M.code)` (System.Mod lines 320–321, 404, 411).

**Likely root cause (TBD):** Either:
- `ORD(M)` where `M` is a `Modules.Module` pointer is not emitting `ptrtoint` — the pointer value is passed raw instead of converted to i64; OR
- `M.code` is a field whose resolved type comes out as a pointer type instead of INTEGER (e.g. codegen resolves it as `ptr` because the field type in the loaded Modules.Mod interface is wrong).

**Investigation needed:**
1. Check which specific call site produces the bad IR (add debugging or look at the generated .ll).
2. Check `Modules.ModDesc`'s `code` field type — if it's correctly typed as INTEGER.
3. Check how `ORD()` is handled in codegen: does it emit `ptrtoint` for pointer arguments?

**Unblocks:** System ✅ (last module, enables full 14/14 compilation).

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
