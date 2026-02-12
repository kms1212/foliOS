OUTPUT_ARCH(i386:x86-64)
OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")

ENTRY(_start)

SECTIONS
{
    . = 0x00007FFF80000000;  /* module area */

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
        PROVIDE_HIDDEN(__bss_start = .);
        *(.bss .bss.*)
        PROVIDE_HIDDEN(__bss_end = .);
    }

    . = 0xFFFF800000000000;  /* module area */

    .text :
    {
        PROVIDE_HIDDEN(__text_start = .);
        KEEP(*(.text .text.*))
        PROVIDE_HIDDEN(__text_end = .);
    }

    . = ALIGN(4096);
    .rodata :
    {
        PROVIDE_HIDDEN(__rodata_start = .);
        KEEP(*(.rodata.krt_table))
        KEEP(*(.rodata .rodata.*))
        PROVIDE_HIDDEN(__rodata_end = .);
    }

    PROVIDE_HIDDEN(__end = .);
}
