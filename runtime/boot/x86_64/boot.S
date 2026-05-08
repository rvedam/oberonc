/* x86-64 boot stub: multiboot2 header + 32→64 mode switch
 *
 * Boot sequence
 * ─────────────
 * 1. GRUB drops us here in 32-bit protected mode with a flat 4 GiB segment.
 * 2. Zero BSS now (32-bit, no paging) so page tables start clean.
 * 3. Build identity-map page tables (first 1 GiB, 2 MiB huge pages).
 * 4. Enable PAE + long mode + paging; far-jump to 64-bit code.
 * 5. Reload segments, set up the stack, call hal_init → oberon_main → hal_halt.
 *
 * Zeroing BSS before enabling paging avoids a triple-fault that would
 * otherwise occur when rep stosb exhausts the software TLB and the MMU
 * tries to walk the by-then-zeroed page tables.
 */

/* Multiboot2: recognized by GRUB2 */
.set MULTIBOOT2_MAGIC,    0xE85250D6
.set MULTIBOOT2_ARCH_I386, 0
.set MULTIBOOT2_HDRLEN,   16
.set MULTIBOOT2_CHECKSUM, -(MULTIBOOT2_MAGIC + MULTIBOOT2_ARCH_I386 + MULTIBOOT2_HDRLEN)

/* -----------------------------------------------------------------------
 * PVH ELF note (for QEMU -kernel direct boot without GRUB)
 *
 * QEMU 7.2+ requires either a PVH note or a Multiboot1 header to load
 * a kernel via -kernel.  Multiboot1 rejects 64-bit ELFs, so we use the
 * Xen PVH protocol: a PT_NOTE segment carrying XEN_ELFNOTE_PHYS32_ENTRY
 * (type 18).  QEMU reads the 32-bit entry address, loads all PT_LOAD
 * segments, and jumps to that address in 32-bit protected mode — exactly
 * the state Multiboot2/GRUB also delivers.
 * ----------------------------------------------------------------------- */
.section .note.xen, "a", @note
.align 4
pvh_note:
    .long  4               /* namesz: 4 bytes for "Xen\0" */
    .long  4               /* descsz: 4 bytes for the entry point */
    .long  18              /* type:   XEN_ELFNOTE_PHYS32_ENTRY  */
    .ascii "Xen\0"         /* name (already 4-byte aligned)     */
    .long  _start          /* 32-bit physical entry point       */

/* -----------------------------------------------------------------------
 * Multiboot2 header (must be within the first 32 KiB, 8-byte aligned)
 * Kept for GRUB2 boot compatibility.
 * ----------------------------------------------------------------------- */
.section .multiboot2, "a"
.align 8
multiboot2_header:
    .long  MULTIBOOT2_MAGIC
    .long  MULTIBOOT2_ARCH_I386
    .long  MULTIBOOT2_HDRLEN
    .long  (MULTIBOOT2_CHECKSUM & 0xFFFFFFFF)
    /* end tag */
    .short 0
    .short 0
    .long  8

/* -----------------------------------------------------------------------
 * Page tables – in BSS (zeroed by us in step 2 before we fill them in)
 * ----------------------------------------------------------------------- */
.section .bss
.align 4096
pml4:   .space 4096
pdpt:   .space 4096
pd0:    .space 4096          /* 0x000000000 – 0x03FFFFFFF (first  1 GiB) */
pd1:    .space 4096          /* 0x040000000 – 0x07FFFFFFF (second 1 GiB) */
pd2:    .space 4096          /* 0x080000000 – 0x0BFFFFFFF (third  1 GiB) */
pd3:    .space 4096          /* 0x0C0000000 – 0x0FFFFFFFF (fourth 1 GiB, covers 0xFD000000 VBE LFB) */

/* Alias pd → pd0 for backward compatibility */
.set pd, pd0

/* 64 KiB stack */
.align 16
_stack_bot: .space 65536
_stack_top:

.section .note.GNU-stack,"",@progbits

.extern _bss_start
.extern _bss_end

/* -----------------------------------------------------------------------
 * GDT for long mode
 * ----------------------------------------------------------------------- */
.section .data
.align 8
gdt64:
    .quad 0x0000000000000000          /* null descriptor */
    .quad 0x00AF9A000000FFFF          /* 64-bit code: L=1, P=1, DPL=0 */
    .quad 0x00CF92000000FFFF          /* data: D=1, P=1, DPL=0         */
gdt64_end:

gdt64_ptr:
    .word gdt64_end - gdt64 - 1
    .long gdt64

/* -----------------------------------------------------------------------
 * 32-bit entry point
 * ----------------------------------------------------------------------- */
.section .text
.code32
.global _start
_start:
    cli
    /* Save multiboot2 info pointer (unused beyond this point) */
    movl    %ebx, %esi

    /* ------------------------------------------------------------------ */
    /* Step 1: Zero BSS in 32-bit mode, before paging is enabled.         */
    /*                                                                     */
    /* This includes pml4/pdpt/pd, which we then fill in from scratch.    */
    /* Doing it here avoids invalidating the active page tables while      */
    /* running rep stosb in 64-bit mode (a source of triple-faults when   */
    /* the kernel's BSS is larger than the TLB can cover).                */
    /* ------------------------------------------------------------------ */
    movl    $_bss_start, %edi
    movl    $_bss_end,   %ecx
    subl    %edi, %ecx
    xorl    %eax, %eax
    rep stosb

    /* ------------------------------------------------------------------ */
    /* Step 2: Build identity-map page tables (first 4 GiB, 2 MiB pages) */
    /*                                                                     */
    /* 4 GiB covers the VBE linear framebuffer at ~0xFD000000 so that     */
    /* hal_display_flush can write to the Bochs VBE LFB on QEMU.          */
    /* ------------------------------------------------------------------ */

    /* PML4[0] → PDPT */
    movl    $pdpt, %eax
    orl     $0x3, %eax
    movl    %eax, pml4

    /* PDPT[0..3] → pd0..pd3 (one 4 KiB PD per GiB) */
    movl    $pd0, %eax
    orl     $0x3, %eax
    movl    %eax, pdpt+0        /* PDPT[0] */

    movl    $pd1, %eax
    orl     $0x3, %eax
    movl    %eax, pdpt+8        /* PDPT[1] */

    movl    $pd2, %eax
    orl     $0x3, %eax
    movl    %eax, pdpt+16       /* PDPT[2] */

    movl    $pd3, %eax
    orl     $0x3, %eax
    movl    %eax, pdpt+24       /* PDPT[3] */

    /* Fill all 4 PDs: 4 × 512 entries = 2048 × 2 MiB = 4 GiB */
    /* ECX = global page index 0..2047; page base = ECX << 21   */
    movl    $0, %ecx
2:
    movl    %ecx, %eax
    shll    $21, %eax           /* 2 MiB page base (low 32 bits) */
    orl     $0x83, %eax         /* present | writable | huge */

    /* Select the right PD: pd0 for ECX 0..511, pd1 for 512..1023, etc. */
    movl    %ecx, %edx
    shrl    $9, %edx            /* EDX = ECX / 512 = PD index (0..3)  */
    movl    %ecx, %esi
    andl    $511, %esi          /* ESI = ECX MOD 512 = entry in PD     */

    /* Compute PD base address: pd0 + EDX*4096 */
    movl    $pd0, %edi
    imull   $4096, %edx, %edx
    addl    %edx, %edi

    movl    %eax, (%edi,%esi,8)
    movl    $0,   4(%edi,%esi,8)

    incl    %ecx
    cmpl    $2048, %ecx
    jne     2b

    /* ------------------------------------------------------------------ */
    /* Step 3: Enable PAE + long mode + paging                            */
    /* ------------------------------------------------------------------ */
    movl    $pml4, %eax
    movl    %eax, %cr3

    movl    %cr4, %eax
    orl     $0x20, %eax         /* PAE */
    movl    %eax, %cr4

    movl    $0xC0000080, %ecx
    rdmsr
    orl     $0x100, %eax        /* LME */
    wrmsr

    movl    %cr0, %eax
    orl     $0x80000001, %eax   /* PG | PE */
    movl    %eax, %cr0

    lgdt    gdt64_ptr
    ljmpl   $0x08, $_start64

/* -----------------------------------------------------------------------
 * 64-bit entry point
 * ----------------------------------------------------------------------- */
.code64
_start64:
    movw    $0x10, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %ss
    xorw    %ax, %ax
    movw    %ax, %fs
    movw    %ax, %gs

    movabsq $_stack_top, %rsp

    /* BSS is already zeroed (done in 32-bit mode above) */

    call    hal_init
    call    oberon_main
    call    hal_display_flush   /* blit 1bpp shadow → VBE 8bpp LFB */
    call    hal_halt

.Lhalt64:
    cli
    hlt
    jmp     .Lhalt64
