OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

/*
 * PHDRS: explicit program headers so the linker emits a PT_NOTE segment.
 * QEMU's -kernel loader scans PT_NOTE to find XEN_ELFNOTE_PHYS32_ENTRY.
 * .note.xen is placed in both :note and :text so it lands in memory (via
 * the PT_LOAD) and is also discoverable via PT_NOTE.
 */
PHDRS {
    note PT_NOTE FLAGS(4);   /* read-only; carries the PVH ELF note */
    text PT_LOAD FLAGS(7);   /* RWX text + data + note data         */
    bss  PT_LOAD FLAGS(6);   /* RW  BSS                             */
}

SECTIONS {
    . = 0x100000;

    /* PVH note: in PT_NOTE (for QEMU discovery) AND PT_LOAD (for runtime) */
    .note.xen   : { *(.note.xen)   } :note :text

    /* Multiboot2 header must come first within first 32 KiB */
    .multiboot2 : { *(.multiboot2) } :text

    .text   : { *(.text .text.*)       } :text
    .rodata : { *(.rodata .rodata.*)   } :text
    .data   : { *(.data   .data.*)     } :text

    .bss : {
        _bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        _bss_end = .;
    } :bss
}
