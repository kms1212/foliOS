OUTPUT_ARCH(i386:x86-64)
OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")

ENTRY(_start)

SECTIONS
{
    . = 0x00000000C0000000;  /* trampoline area */

    .trampoline :
    {
        PROVIDE_HIDDEN(__trampoline_start = .);
        KEEP(*(.trampoline .trampoline.*))
        PROVIDE_HIDDEN(__trampoline_end = .);
    }

    __trampoline_load = 0x0000000000100000;
    __trampoline_size = SIZEOF(.trampoline);

    .text.startup :
    {
        PROVIDE_HIDDEN(__text_startup_start = .);
        KEEP(*(.text.startup .text.startup.*))
        PROVIDE_HIDDEN(__text_startup_end = .);
    }

    /* kernel area */
    . = 0xFFFFFFFF80000000 + ADDR(.text.startup) + SIZEOF(.text.startup) - ADDR(.trampoline);

    .text : AT(ADDR(.text) - ADDR(.trampoline))
    {
        PROVIDE_HIDDEN(__text_start = .);
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

    . = ALIGN(4096);
    .bss :
    {
        PROVIDE_HIDDEN(__bss_start = .);
        *(.bss .bss.*)
        PROVIDE_HIDDEN(__bss_end = .);
    }

    PROVIDE_HIDDEN(__end = .);
}
