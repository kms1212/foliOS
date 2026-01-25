OUTPUT_ARCH(i386)
OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")

ENTRY(_start)

SECTIONS
{
    . = 0x00100000;
    
    .text :
    {
        PROVIDE_HIDDEN(_text_start_ = .);
        KEEP(*(.text.startup .text.startup.*))
        KEEP(*(.text .text.*))
        PROVIDE_HIDDEN(_text_end_ = .);
    }

    .data :
    {
        . = ALIGN(16);
        PROVIDE_HIDDEN(_data_start_ = .);
        *(.data .data.*)
        PROVIDE_HIDDEN(_data_end_ = .);

        . = ALIGN(16);
        PROVIDE(_init_array_start_ = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
        KEEP(*(.init_array .ctors))
        PROVIDE(_init_array_end_ = .);

        . = ALIGN(16);
        PROVIDE_HIDDEN(_fini_array_start_ = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
        KEEP(*(.fini_array .dtors))
        PROVIDE_HIDDEN(_fini_array_end_ = .);

        . = ALIGN(16);
        KEEP(*(SORT_NONE(.init)))

        . = ALIGN(16);
        KEEP(*(SORT_NONE(.fini)))

        . = ALIGN(16);
        KEEP(*(SORT(.ctors.*)))
        KEEP(*(.ctors))

        . = ALIGN(16);
        KEEP(*(SORT(.dtors.*)))
        KEEP(*(.dtors))
    }

    . = ALIGN(4096);
    .rodata :
    {
        PROVIDE_HIDDEN(_rodata_start_ = .);
        KEEP(*(.rodata .rodata.*))
        PROVIDE_HIDDEN(_rodata_end_ = .);
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
