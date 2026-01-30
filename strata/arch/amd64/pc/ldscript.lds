OUTPUT_ARCH(i386:x86-64)
OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")

ENTRY(_start)

SECTIONS
{
    . = 0x00000000C0000000;  /* trampoline area */

    .trampoline :
    {
        PROVIDE_HIDDEN(_trampoline_start_ = .);
        KEEP(*(.trampoline .trampoline.*))
        PROVIDE_HIDDEN(_trampoline_end_ = .);
    }

    _trampoline_load_ = 0x0000000000100000;
    _trampoline_size_ = SIZEOF(.trampoline);

    .text.startup :
    {
        PROVIDE_HIDDEN(_text_startup_start = .);
        KEEP(*(.text.startup .text.startup.*))
        PROVIDE_HIDDEN(_text_startup_end = .);
    }

    /* kernel area */
    . = 0xFFFFFFFF80000000 + ADDR(.text.startup) + SIZEOF(.text.startup) - ADDR(.trampoline);

    .text : AT(ADDR(.text) - ADDR(.trampoline))
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

    . = ALIGN(4096);
    .data :
    {
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
    .bss :
    {
        PROVIDE_HIDDEN(_bss_start_ = .);
        *(.bss .bss.*)
        PROVIDE_HIDDEN(_bss_end_ = .);
    }

    PROVIDE_HIDDEN(_end_ = .);
}
