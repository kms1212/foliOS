OUTPUT_ARCH(i386:x86-64)
OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")

ENTRY(_start)

SECTIONS
{
    . = 0x00007FFF80000000;  /* module area */

    . = ALIGN(4096);
    .data :
    {
        PROVIDE_HIDDEN(_data_start_ = .);
        *(.data .data.*)
        PROVIDE_HIDDEN(_data_end_ = .);
    }

    . = ALIGN(4096);
    .bss :
    {
        PROVIDE_HIDDEN(_bss_start_ = .);
        *(.bss .bss.*)
        PROVIDE_HIDDEN(_bss_end_ = .);
    }

    . = 0xFFFF800000000000;  /* module area */

    .text :
    {
        PROVIDE_HIDDEN(_text_start_ = .);
        KEEP(*(.text .text.*))
        PROVIDE_HIDDEN(_text_end_ = .);
    }

    . = ALIGN(4096);
    .rodata :
    {
        PROVIDE_HIDDEN(_rodata_start_ = .);
        KEEP(*(.rodata .rodata.*))
        PROVIDE_HIDDEN(_rodata_end_ = .);
    }

    PROVIDE_HIDDEN(_end_ = .);
}
