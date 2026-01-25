OUTPUT_ARCH(i386)
OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")

ENTRY(_start)

SECTIONS
{
    . = 0x00001000;
    .bss :
    {
        PROVIDE_HIDDEN(_bss_start_ = .);
        *(.bss .bss.*)
        *(COMMON)
        PROVIDE_HIDDEN(_bss_end_ = .);
    }

    . = 0x00007C00;
    .text :
    {
        KEEP(*(.text .text.*))
    }

    .data :
    {
        *(.data .data.*)
    }
    
    .rodata :
    {
        KEEP(*(.rodata .rodata.*))
    }
}
