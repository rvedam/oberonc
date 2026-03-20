/* x86-64 boot stub: multiboot2 header + 32→64 mode switch */
.set MULTIBOOT2_MAGIC,    0xE85250D6
.set MULTIBOOT2_ARCH_I386, 0
.set MULTIBOOT2_HDRLEN,   16
.set MULTIBOOT2_CHECKSUM, -(MULTIBOOT2_MAGIC + MULTIBOOT2_ARCH_I386 + MULTIBOOT2_HDRLEN)

/* -----------------------------------------------------------------------
 * Multiboot2 header (must be in first 32 KiB of the image, 8-byte aligned)
 * ----------------------------------------------------------------------- */
.section .multiboot2, "a"
.align 8
multiboot2_header:
    .long  MULTIBOOT2_MAGIC
    .long  MULTIBOOT2_ARCH_I386
    .long  MULTIBOOT2_HDRLEN
    .long  (MULTIBOOT2_CHECKSUM & 0xFFFFFFFF)
    /* end tag (type=0, flags=0, size=8) */
    .short 0
    .short 0
    .long  8

/* -----------------------------------------------------------------------
 * Page tables (identity-map first 1 GiB using 2 MiB huge pages)
 * Located in BSS so they are 4-KiB aligned sections of zeros initially.
 * We fill them in _start before enabling paging.
 * ----------------------------------------------------------------------- */
.section .bss
.align 4096
pml4:   .space 4096
pdpt:   .space 4096
pd:     .space 4096

/* 64 KiB stack */
.align 16
_stack_bot: .space 65536
_stack_top:

/* Tell the linker this code does not require an executable stack */
.section .note.GNU-stack,"",@progbits

/* BSS markers (defined by linker script — just declared here for asm use) */
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
    .long gdt64                       /* 32-bit address (fits in low 4 GiB) */

/* -----------------------------------------------------------------------
 * 32-bit entry point (GRUB/QEMU drops us here in protected mode)
 * ----------------------------------------------------------------------- */
.section .text
.code32
.global _start
_start:
    cli
    /* Save multiboot2 info pointer from %ebx (not used now, but kept) */
    movl    %ebx, %esi

    /* ---- Build identity-map page tables -------------------------------- */

    /* PML4[0] → PDPT (present | writable) */
    movl    $pdpt, %eax
    orl     $0x3, %eax
    movl    %eax, pml4

    /* PDPT[0] → PD (present | writable) */
    movl    $pd, %eax
    orl     $0x3, %eax
    movl    %eax, pdpt

    /* PD[0..511]: 512 × 2 MiB huge pages (present | writable | PS) */
    movl    $0, %ecx
1:
    movl    %ecx, %eax
    shll    $21, %eax           /* page base address (lower 32 bits) */
    orl     $0x83, %eax         /* present | writable | huge */
    movl    %eax, pd(,%ecx,8)
    movl    $0, pd+4(,%ecx,8)
    incl    %ecx
    cmpl    $512, %ecx
    jne     1b

    /* ---- Enable PAE + long mode + paging ------------------------------- */
    movl    $pml4, %eax
    movl    %eax, %cr3

    /* CR4: PAE (bit 5) */
    movl    %cr4, %eax
    orl     $0x20, %eax
    movl    %eax, %cr4

    /* EFER: LME (bit 8) via MSR 0xC0000080 */
    movl    $0xC0000080, %ecx
    rdmsr
    orl     $0x100, %eax
    wrmsr

    /* CR0: paging (bit 31) + protected mode (bit 0) */
    movl    %cr0, %eax
    orl     $0x80000001, %eax
    movl    %eax, %cr0

    /* Far jump to flush prefetch queue and activate 64-bit mode */
    lgdt    gdt64_ptr
    ljmpl   $0x08, $_start64

/* -----------------------------------------------------------------------
 * 64-bit entry point
 * ----------------------------------------------------------------------- */
.code64
_start64:
    /* Load data segment into remaining segment registers */
    movw    $0x10, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %ss
    xorw    %ax, %ax
    movw    %ax, %fs
    movw    %ax, %gs

    /* Set up stack */
    movabsq $_stack_top, %rsp

    /* Zero BSS */
    movabsq $_bss_start, %rdi
    movabsq $_bss_end,   %rcx
    subq    %rdi, %rcx
    xorl    %eax, %eax
    rep stosb

    /* Call runtime */
    call    hal_init
    call    oberon_main
    call    hal_halt

    /* Should never reach here */
.Lhalt64:
    cli
    hlt
    jmp     .Lhalt64
