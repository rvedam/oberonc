/* AArch64 boot stub for QEMU virt machine.
 * Entry at _start from EL2 (QEMU default) or EL1.
 * Drops to EL1, zeroes BSS, then calls hal_init → oberon_main → hal_halt.
 */

.section .text
.global _start
_start:
    /* Mask all interrupts */
    msr     DAIFSet, #0xf

    /* Determine current EL */
    mrs     x0, CurrentEL
    lsr     x0, x0, #2
    cmp     x0, #2
    bne     .Lat_el1        /* already at EL1, skip the drop */

    /* Configure HCR_EL2: set RW bit (EL1 is AArch64) */
    mov     x0, #(1 << 31)
    msr     hcr_el2, x0

    /* Set SPSR_EL2 to return to EL1h with interrupts masked */
    mov     x0, #0x3C5     /* DAIF=1111, M[3:0]=0101 (EL1h) */
    msr     spsr_el2, x0

    /* Set ELR_EL2 to the EL1 entry point */
    adr     x0, .Lat_el1
    msr     elr_el2, x0

    eret                    /* drops to EL1 */

.Lat_el1:
    /* Set stack pointer */
    adrp    x0, _stack_top
    add     x0, x0, :lo12:_stack_top
    mov     sp, x0

    /* Install the exception vectors (report faults on the UART) */
    adrp    x0, _vectors
    add     x0, x0, :lo12:_vectors
    msr     vbar_el1, x0
    isb

    /* Enable the MMU with an identity map (see _page_table below), so RAM is
     * Normal cacheable memory: unaligned accesses are allowed and the caches
     * work. With the MMU off every access is Device-nGnRnE.
     *   MAIR:  attr0 = 0x00 Device-nGnRnE, attr1 = 0xFF Normal write-back
     *   TCR:   T0SZ = 32 (4 GiB of VA, lookup starts at level 1), 4 KiB
     *          granule, inner/outer write-back, inner shareable, TTBR1 walks
     *          disabled (EPD1), 32-bit physical addresses (IPS = 0) */
    mov     x0, #0xFF00
    msr     mair_el1, x0
    ldr     x0, =(32 | (1 << 8) | (1 << 10) | (3 << 12) | (1 << 23))
    msr     tcr_el1, x0
    adrp    x0, _page_table
    add     x0, x0, :lo12:_page_table
    msr     ttbr0_el1, x0
    isb
    tlbi    vmalle1
    dsb     sy
    isb
    mrs     x0, sctlr_el1
    orr     x0, x0, #(1 << 0)       /* M: MMU on */
    orr     x0, x0, #(1 << 2)       /* C: data cache */
    orr     x0, x0, #(1 << 12)      /* I: instruction cache */
    bic     x0, x0, #(1 << 1)       /* A: no alignment checking */
    msr     sctlr_el1, x0
    isb

    /* Zero BSS */
    adrp    x1, _bss_start
    add     x1, x1, :lo12:_bss_start
    adrp    x2, _bss_end
    add     x2, x2, :lo12:_bss_end
.Lbss_loop:
    cmp     x1, x2
    bge     .Lbss_done
    str     xzr, [x1], #8
    b       .Lbss_loop
.Lbss_done:

    bl      hal_init
    bl      oberon_main
    bl      hal_display_flush
    bl      hal_halt

    /* Should never reach here */
.Lhalt:
    wfi
    b       .Lhalt

/* -----------------------------------------------------------------------
 * Exception vector table: 16 entries of 0x80 bytes. Every exception is
 * fatal here; hal_exception(kind, ESR, ELR, FAR) prints it and halts.
 * ----------------------------------------------------------------------- */
.macro VECTOR kind
    .balign 0x80
    mov     x0, #\kind
    b       .Lexception
.endm

.balign 0x800
_vectors:
    VECTOR 0
    VECTOR 1
    VECTOR 2
    VECTOR 3
    VECTOR 4
    VECTOR 5
    VECTOR 6
    VECTOR 7
    VECTOR 8
    VECTOR 9
    VECTOR 10
    VECTOR 11
    VECTOR 12
    VECTOR 13
    VECTOR 14
    VECTOR 15

.Lexception:
    mrs     x1, esr_el1
    mrs     x2, elr_el1
    mrs     x3, far_el1
    bl      hal_exception
    b       .Lhalt

/* -----------------------------------------------------------------------
 * Level-1 translation table: four 1 GiB identity-mapped blocks.
 *   0x00000000  Device, never executable (flash, UART, fw_cfg, PCIe MMIO)
 *   0x40000000+ Normal write-back, inner shareable (RAM on QEMU virt)
 * Block descriptor: bit 0 valid, bit 1 = 0 block, AttrIndx in [4:2],
 * SH in [9:8], AF bit 10, PXN bit 53, UXN bit 54.
 * ----------------------------------------------------------------------- */
.section .data
.balign 4096
_page_table:
    .quad 0x00000000 | (1 << 54) | (1 << 53) | (1 << 10) | (0 << 2) | 1
    .quad 0x40000000 | (1 << 10) | (3 << 8) | (1 << 2) | 1
    .quad 0x80000000 | (1 << 10) | (3 << 8) | (1 << 2) | 1
    .quad 0xC0000000 | (1 << 10) | (3 << 8) | (1 << 2) | 1

/* -----------------------------------------------------------------------
 * Stack (1 MiB; Project Oberon procedures keep 1 KiB sector buffers on the
 * stack and recurse) and BSS markers — the linker script exposes
 * _bss_start/_bss_end
 * ----------------------------------------------------------------------- */
.section .bss
.align 16
_stack_bot: .space 1048576
_stack_top:

/* Tell the linker this code does not require an executable stack */
.section .note.GNU-stack,"",@progbits
