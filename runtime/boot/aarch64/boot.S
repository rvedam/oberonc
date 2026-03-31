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
 * Stack (64 KiB) and BSS markers — linker script exposes _bss_start/_bss_end
 * ----------------------------------------------------------------------- */
.section .bss
.align 16
_stack_bot: .space 65536
_stack_top:

/* Tell the linker this code does not require an executable stack */
.section .note.GNU-stack,"",@progbits
