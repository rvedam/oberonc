OUTPUT_FORMAT(elf64-littleaarch64)
ENTRY(_start)

SECTIONS {
    . = 0x40080000;

    .text : { *(.text .text.*) }

    .rodata : { *(.rodata .rodata.*) }
    .data   : { *(.data   .data.*)   }

    .bss : {
        _bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        _bss_end = .;
    }
}
