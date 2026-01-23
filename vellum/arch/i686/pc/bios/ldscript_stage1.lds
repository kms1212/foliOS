OUTPUT_ARCH(i386)
OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")

ENTRY(_s1start)

SECTIONS
{
    . = 0x00001000;
    .bss :
    {
        PROVIDE_HIDDEN(__bss_start = .);
        *(.bss .bss.*)
        *(COMMON)
        PROVIDE_HIDDEN(__bss_end = .);
    }

    . = 0x00009000;
    .text :
    {
        PROVIDE_HIDDEN(__text_start = .);
        KEEP(*(.text.startup .text.startup.*))
        KEEP(*(.text.low .text.low.*))
        KEEP(*(.text .text.*))
        PROVIDE_HIDDEN(__text_end = .);
    }

    .data :
    {
        . = ALIGN(16);
        PROVIDE_HIDDEN(__data_start = .);
        *(.data .data.*)
        PROVIDE_HIDDEN(__data_end = .);

        . = ALIGN(16);
        PROVIDE(__init_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
        KEEP(*(.init_array .ctors))
        PROVIDE(__init_array_end = .);

        . = ALIGN(16);
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
        KEEP(*(.fini_array .dtors))
        PROVIDE_HIDDEN(__fini_array_end = .);

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

    . = ALIGN(16);
    .rodata :
    {
        PROVIDE_HIDDEN(__rodata_start = .);
        KEEP(*(.rodata .rodata.*))
        PROVIDE_HIDDEN(__rodata_end = .);
    }

    PROVIDE_HIDDEN(__end = .);
    __stage1_end = .;
}
