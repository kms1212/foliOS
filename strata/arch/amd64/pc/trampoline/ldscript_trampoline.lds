OUTPUT_ARCH(i386)
OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")

ENTRY(_start)

SECTIONS
{
    . = 0x0000000000100000;

    .text :
    {
        PROVIDE_HIDDEN(_text_start_ = .);
        KEEP(*(.text.startup .text.startup.*))
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

    . = ALIGN(4096);
    .data :
    {
        PROVIDE_HIDDEN(_data_start_ = .);
        KEEP(*(.data .data.*))
        PROVIDE_HIDDEN(_data_end_ = .);
    }

    . = ALIGN(4096);
    .bss :
    {
        PROVIDE_HIDDEN(_bss_start_ = .);
        *(.bss .bss.*)
        PROVIDE_HIDDEN(_bss_end_ = .);
    }

    PROVIDE_HIDDEN(_end_ = .);
}
