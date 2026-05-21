OUTPUT_ARCH(i386:x86-64)
OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")

ENTRY(g_entries)

#include <strata/plat/memmap.h>

SECTIONS
{
    . = MEMMAP_KRT_DATA_ADDR_BASE;

    . = ALIGN(4096);
    .data :
    {
        PROVIDE_HIDDEN(__data_start = .);
        *(.data .data.*)
        PROVIDE_HIDDEN(__data_end = .);
    }

    . = ALIGN(4096);
    .bss :
    {
        PROVIDE_HIDDEN(___bss_start = .);
        *(.bss .bss.*)
        PROVIDE_HIDDEN(___bss_end = .);
    }

    . = MEMMAP_KRT_TEXT_ADDR_BASE;

    .rodata :
    {
        PROVIDE_HIDDEN(__rodata_start = .);
        KEEP(*(.rodata.krt_table))
        KEEP(*(.rodata .rodata.*))
        PROVIDE_HIDDEN(__rodata_end = .);
    }

    . = ALIGN(4096);
    .text :
    {
        PROVIDE_HIDDEN(___text_start = .);
        KEEP(*(.text .text.*))
        PROVIDE_HIDDEN(__text_end = .);
    }

    PROVIDE_HIDDEN(__end = .);
}
