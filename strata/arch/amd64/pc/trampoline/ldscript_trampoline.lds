OUTPUT_ARCH(i386)
OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")

ENTRY(_start)

#include <strata/plat/image_layout.h>

SECTIONS
{
    . = STRATA_TRAMPOLINE_LOAD_ADDR_BASE;

    .text :
    {
        PROVIDE_HIDDEN(___text_start = .);
        KEEP(*(.text.startup .text.startup.*))
        KEEP(*(.text .text.*))
        PROVIDE_HIDDEN(__text_end = .);
    }
    
    . = ALIGN(4096);
    .rodata :
    {
        PROVIDE_HIDDEN(__rodata_start = .);
        KEEP(*(.rodata .rodata.*))
        PROVIDE_HIDDEN(__rodata_end = .);
    }

    . = ALIGN(4096);
    .data :
    {
        PROVIDE_HIDDEN(__data_start = .);
        KEEP(*(.data .data.*))
        PROVIDE_HIDDEN(__data_end = .);
    }

    . = ALIGN(4096);
    .bss :
    {
        PROVIDE_HIDDEN(___bss_start = .);
        *(.bss .bss.*)
        PROVIDE_HIDDEN(___bss_end = .);
    }

    PROVIDE_HIDDEN(__end = .);
}
