#!/usr/bin/env bash
# build_kernel.sh — compile Oberon modules into a bootable bare-metal image
#
# Usage:
#   ./build_kernel.sh [OPTIONS] MAIN.Mod [DEP.Mod ...]
#
# Options:
#   -a, --arch ARCH     Target architecture: x86_64 (default) or aarch64
#   -o, --output FILE   Output file (default: kernel_<arch>.iso / kernel_<arch>.elf)
#   --iso               Force ISO output even if grub-mkrescue is unavailable
#   --elf               Produce a raw ELF only (skip ISO creation)
#   --debug-info        Emit DWARF debug info (passes --debugger-tune=gdb --dwarf-version=4 to llc)
#   --run               Launch QEMU after building (add debugging flags below as needed)
#   --gdb               With --run: add GDB stub (-s -S); pause VM until GDB connects
#                       Connect with: gdb -ex "target remote :1234"
#   --qmp               With --run: add QMP JSON socket at ./qmp.sock
#                       Query with: echo '{"execute":"qmp_capabilities"}' | nc -U ./qmp.sock
#   --monitor           With --run: expose QEMU monitor on ./mon.sock
#                       Connect with: nc -U ./mon.sock  (then: info registers, x/10i $rip, ...)
#   -h, --help          Show this help
#
# The first .Mod file is the "main" module (provides oberon_main).
# Any additional .Mod files are compiled as dependency modules (--init-only).
#
# Requirements:
#   - cmake-built tree at ./build/  (cmake -B build && cmake --build build)
#   - llc-14 (x86_64) or llc (aarch64)
#   - ld (binutils, same arch as target)
#   - grub-mkrescue + xorriso (for ISO output on x86_64)
#   - aarch64-linux-gnu-{g++,ld} (for aarch64 cross-compilation)
#   - gdb (optional, for --gdb)
#   - qemu-system-{x86_64,aarch64} (for --run)

set -euo pipefail

# ---------------------------------------------------------------------------
# Defaults
# ---------------------------------------------------------------------------
ARCH=x86_64
OUTPUT=""
FORCE_ISO=0
ELF_ONLY=0
DEBUG_INFO=0
RUN_QEMU=0
GDB_STUB=0
QMP_SOCK=0
MONITOR_SOCK=0
MAIN_MOD=""
DEP_MODS=()

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------
while [[ $# -gt 0 ]]; do
    case "$1" in
        -a|--arch)    ARCH="$2"; shift 2 ;;
        -o|--output)  OUTPUT="$2"; shift 2 ;;
        --iso)        FORCE_ISO=1; shift ;;
        --elf)        ELF_ONLY=1; shift ;;
        --debug-info) DEBUG_INFO=1; shift ;;
        --run)        RUN_QEMU=1; shift ;;
        --gdb)        GDB_STUB=1; RUN_QEMU=1; shift ;;
        --qmp)        QMP_SOCK=1; RUN_QEMU=1; shift ;;
        --monitor)    MONITOR_SOCK=1; RUN_QEMU=1; shift ;;
        -h|--help)
            sed -n '2,/^set -/p' "$0" | grep '^#' | sed 's/^# \{0,1\}//'
            exit 0 ;;
        -*)  echo "Unknown option: $1"; exit 1 ;;
        *)
            if [[ -z "$MAIN_MOD" ]]; then
                MAIN_MOD="$1"
            else
                DEP_MODS+=("$1")
            fi
            shift ;;
    esac
done

if [[ -z "$MAIN_MOD" ]]; then
    echo "error: no main module specified"
    echo "usage: $0 [OPTIONS] MAIN.Mod [DEP.Mod ...]"
    exit 1
fi

# ---------------------------------------------------------------------------
# Validate architecture
# ---------------------------------------------------------------------------
case "$ARCH" in
    x86_64|aarch64) ;;
    *) echo "error: unknown arch '$ARCH' (valid: x86_64, aarch64)"; exit 1 ;;
esac

# ---------------------------------------------------------------------------
# Locate tools
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
OBERONC="$BUILD_DIR/oberonc"

if [[ ! -x "$OBERONC" ]]; then
    echo "error: compiler not found at $OBERONC"
    echo "  Run: cmake -B build && cmake --build build"
    exit 1
fi

# llc: prefer llc-14 for x86_64 (matches LLVM version used to build oberonc)
find_llc() {
    for c in llc-14 llc; do
        if command -v "$c" &>/dev/null; then echo "$c"; return; fi
    done
    echo ""
}
LLC="$(find_llc)"
if [[ -z "$LLC" ]]; then
    echo "error: llc not found (install llvm-14 or llvm)"; exit 1
fi

# ---------------------------------------------------------------------------
# Derive paths from the main module name
# ---------------------------------------------------------------------------
MAIN_STEM="${MAIN_MOD%.Mod}"
MAIN_NAME="$(basename "$MAIN_STEM")"

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

ELF_OUT="${OUTPUT:-$SCRIPT_DIR/build/kernel_${ARCH}.elf}"
ISO_OUT="${ELF_OUT%.elf}.iso"

# ---------------------------------------------------------------------------
# Helper: compile one .Mod to LLVM IR  (prints progress to stderr, echoes path)
# ---------------------------------------------------------------------------
compile_mod() {
    local modfile="$1"
    local extra_flags="${2:-}"
    local stem="${modfile%.Mod}"
    local llpath="$WORK_DIR/$(basename "$stem").ll"

    echo "  [oberonc] $modfile → $(basename "$llpath")" >&2
    # shellcheck disable=SC2086
    "$OBERONC" --emit-llvm $extra_flags -o "$llpath" "$modfile" >&2
    echo "$llpath"
}

# ---------------------------------------------------------------------------
# Helper: assemble LLVM IR → native object  (prints progress to stderr, echoes path)
# ---------------------------------------------------------------------------
assemble_ll() {
    local llfile="$1"
    local objfile="$WORK_DIR/$(basename "${llfile%.ll}").o"

    echo "  [llc]     $(basename "$llfile") → $(basename "$objfile")" >&2
    local debug_flags=""
    [[ "$DEBUG_INFO" -eq 1 ]] && debug_flags="--debugger-tune=gdb --dwarf-version=4"
    # shellcheck disable=SC2086
    "$LLC" --march="$LLC_ARCH" --filetype=obj \
           --relocation-model=static \
           $debug_flags \
           "$llfile" -o "$objfile" >&2
    echo "$objfile"
}

# ---------------------------------------------------------------------------
# Architecture-specific settings
# ---------------------------------------------------------------------------
case "$ARCH" in
x86_64)
    LLC_ARCH="x86-64"
    LINKER="ld"
    LD_EMUL="-m elf_x86_64"
    LINKER_SCRIPT="$SCRIPT_DIR/runtime/boot/x86_64/kernel.ld"
    BOOT_OBJ="$BUILD_DIR/CMakeFiles/boot_bare.dir/runtime/boot/x86_64/boot.S.o"
    RUNTIME_LIB="$BUILD_DIR/liboberon_runtime_bare.a"
    ;;
aarch64)
    LLC_ARCH="aarch64"
    LINKER="${AARCH64_LD:-aarch64-linux-gnu-ld}"
    LD_EMUL=""
    LINKER_SCRIPT="$SCRIPT_DIR/runtime/boot/aarch64/kernel.ld"
    BOOT_OBJ="$BUILD_DIR/boot.o"
    RUNTIME_LIB="$BUILD_DIR/liboberon_runtime_bare.a"
    ;;
esac

# ---------------------------------------------------------------------------
# Make sure the cmake-built boot+runtime artefacts are up to date
# ---------------------------------------------------------------------------
echo "==> Building boot stub and bare-metal runtime for $ARCH..."
cmake --build "$BUILD_DIR" \
      --target boot_bare oberon_runtime_bare \
      -- -j"$(nproc)" 2>&1 \
    | grep -E 'Built target|error:' || true

if [[ ! -f "$BOOT_OBJ" ]]; then
    echo "error: boot object not found: $BOOT_OBJ"
    exit 1
fi
if [[ ! -f "$RUNTIME_LIB" ]]; then
    echo "error: runtime library not found: $RUNTIME_LIB"
    exit 1
fi

# ---------------------------------------------------------------------------
# Compile modules
# ---------------------------------------------------------------------------
OBJ_FILES=()

echo ""
echo "==> Compiling Oberon modules..."

# Dependency modules (--init-only: emit ModName_init instead of oberon_main)
for depmod in "${DEP_MODS[@]}"; do
    ll="$(compile_mod "$depmod" "--init-only")"
    obj="$(assemble_ll "$ll")"
    OBJ_FILES+=("$obj")
done

# Main module (provides oberon_main)
ll="$(compile_mod "$MAIN_MOD")"
obj="$(assemble_ll "$ll")"
OBJ_FILES+=("$obj")

# ---------------------------------------------------------------------------
# Link
# ---------------------------------------------------------------------------
echo ""
echo "==> Linking $ARCH ELF kernel → $ELF_OUT..."
# shellcheck disable=SC2086
"$LINKER" $LD_EMUL \
    -T "$LINKER_SCRIPT" \
    "$BOOT_OBJ" \
    "${OBJ_FILES[@]}" \
    "$RUNTIME_LIB" \
    -o "$ELF_OUT" 2>&1 | grep -v "RWX permissions" || true

echo "    ELF: $ELF_OUT  ($(du -h "$ELF_OUT" | cut -f1))"

[[ "$ELF_ONLY" -eq 1 ]] && { echo "Done."; exit 0; }

# ---------------------------------------------------------------------------
# Create bootable ISO (x86_64 via GRUB2 + Multiboot2)
# ---------------------------------------------------------------------------
if [[ "$ARCH" == "x86_64" ]]; then
    if ! command -v grub-mkrescue &>/dev/null; then
        echo ""
        echo "warning: grub-mkrescue not found — skipping ISO creation"
        echo "         Install grub-pc-bin and xorriso to get an ISO."
        echo "         ELF kernel is at: $ELF_OUT"
        exit 0
    fi
    if ! command -v xorriso &>/dev/null; then
        echo ""
        echo "warning: xorriso not found — skipping ISO creation"
        echo "         Install xorriso to get an ISO."
        echo "         ELF kernel is at: $ELF_OUT"
        exit 0
    fi

    ISO_TREE="$WORK_DIR/iso"
    mkdir -p "$ISO_TREE/boot/grub"

    # GRUB config: use serial console so output is visible under QEMU -nographic
    cat > "$ISO_TREE/boot/grub/grub.cfg" << GRUBCFG
set timeout=0
set default=0

# Route GRUB output to the serial port as well as the screen
serial --unit=0 --speed=115200
terminal_input  serial console
terminal_output serial console

menuentry "Project Oberon on $ARCH" {
    multiboot2 /boot/kernel.elf
    boot
}
GRUBCFG

    cp "$ELF_OUT" "$ISO_TREE/boot/kernel.elf"

    echo ""
    echo "==> Creating bootable ISO → $ISO_OUT..."
    grub-mkrescue -o "$ISO_OUT" "$ISO_TREE" 2>&1 \
        | grep -v '^xorriso' | grep -v '^Drive' | grep -v '^Media' \
        | grep -v '^Added' | grep -v '^ISO' | grep -v '^Written' \
        | grep -v "^$" || true

    echo "    ISO: $ISO_OUT  ($(du -h "$ISO_OUT" | cut -f1))"
    echo ""
    echo "==> Run with QEMU:"
    echo "    qemu-system-x86_64 -cdrom $ISO_OUT \\"
    echo "        -serial stdio -vga std -no-reboot -m 128M"

elif [[ "$ARCH" == "aarch64" ]]; then
    echo ""
    echo "==> AArch64 ELF ready (no ISO packaging — boot directly with QEMU):"
    echo "    qemu-system-aarch64 -M virt -cpu cortex-a57 \\"
    echo "        -kernel $ELF_OUT \\"
    echo "        -serial stdio -display none -no-reboot"
fi

# ---------------------------------------------------------------------------
# Launch QEMU (if requested)
# ---------------------------------------------------------------------------
if [[ "$RUN_QEMU" -eq 1 ]]; then
    echo ""
    echo "==> Launching QEMU..."

    # Build QEMU command line
    if [[ "$ARCH" == "x86_64" ]]; then
        QEMU_BIN="qemu-system-x86_64"
        QEMU_KERNEL_ARGS=(-cdrom "$ISO_OUT")
        # -vga std exposes the Bochs VBE extension needed by the display driver.
        # Do NOT use -display none here: the user wants to see the graphical output.
        QEMU_BASE_ARGS=(-serial stdio -vga std -no-reboot -m 128M)
    else
        QEMU_BIN="qemu-system-aarch64"
        QEMU_KERNEL_ARGS=(-M virt -cpu cortex-a57 -kernel "$ELF_OUT")
        QEMU_BASE_ARGS=(-serial stdio -display none -no-reboot)
    fi

    QEMU_DEBUG_ARGS=()

    if [[ "$GDB_STUB" -eq 1 ]]; then
        # -S pauses VM at start; GDB must connect before execution begins
        QEMU_DEBUG_ARGS+=(-s -S)
        echo ""
        echo "    GDB stub enabled on tcp::1234 (VM paused — connect GDB first)"
        echo "    Connect:"
        echo "      gdb build/kernel_${ARCH}.elf \\"
        echo "          -ex 'target remote :1234' \\"
        echo "          -ex 'break *0x100040' \\"
        echo "          -ex 'continue'"
        echo ""
        echo "    Useful GDB commands:"
        echo "      info registers        — dump all CPU registers"
        echo "      x/10i \$rip           — disassemble at current instruction"
        echo "      x/4xg 0x100000        — hex dump physical memory"
        echo "      stepi / nexti         — single-step one instruction"
    fi

    if [[ "$QMP_SOCK" -eq 1 ]]; then
        rm -f ./qmp.sock
        QEMU_DEBUG_ARGS+=(-qmp "unix:./qmp.sock,server,nowait")
        echo ""
        echo "    QMP socket: ./qmp.sock"
        echo "    Query examples:"
        echo "      echo '{\"execute\":\"qmp_capabilities\"}' | nc -U ./qmp.sock"
        echo "      echo '{\"execute\":\"query-status\"}'     | nc -U ./qmp.sock"
        echo "      printf '{\"execute\":\"human-monitor-command\",\"arguments\":{\"command-line\":\"info registers\"}}' | nc -U ./qmp.sock"
    fi

    if [[ "$MONITOR_SOCK" -eq 1 ]]; then
        rm -f ./mon.sock
        QEMU_DEBUG_ARGS+=(-monitor "unix:./mon.sock,server,nowait")
        echo ""
        echo "    Monitor socket: ./mon.sock"
        echo "    Connect: nc -U ./mon.sock"
        echo "    Commands: info registers | x /10i \$rip | xp /4xg 0x100000 | stop | cont"
    fi

    echo ""
    echo "    $ $QEMU_BIN ${QEMU_KERNEL_ARGS[*]} ${QEMU_BASE_ARGS[*]} ${QEMU_DEBUG_ARGS[*]}"
    echo ""

    exec "$QEMU_BIN" "${QEMU_KERNEL_ARGS[@]}" "${QEMU_BASE_ARGS[@]}" "${QEMU_DEBUG_ARGS[@]}"
fi

echo ""
echo "Done."
