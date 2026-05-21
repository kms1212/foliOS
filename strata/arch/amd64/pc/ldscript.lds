OUTPUT_ARCH(i386:x86-64)
OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")

ENTRY(_start)

#include <strata/plat/image_layout.h>
#include <strata/plat/memmap.h>

KERNEL_LOAD_BASE = STRATA_KERNEL_LOAD_ADDR_BASE;
KERNEL_VMA_BASE = MEMMAP_KERNEL_FAST_ADDR_BASE;
KERNEL_VMA_TO_LOAD = KERNEL_VMA_BASE - KERNEL_LOAD_BASE;

SECTIONS
{
    . = KERNEL_LOAD_BASE;  /* trampoline area */

    .trampoline :
    {
        PROVIDE_HIDDEN(__trampoline_start = .);
        KEEP(*(.trampoline .trampoline.*))
        PROVIDE_HIDDEN(__trampoline_end = .);
    }

    __trampoline_load = STRATA_TRAMPOLINE_LOAD_ADDR_BASE;
    __trampoline_size = SIZEOF(.trampoline);

    .text.startup :
    {
        PROVIDE_HIDDEN(___text_startup_start = .);
        KEEP(*(.text.startup))
        PROVIDE_HIDDEN(___text_startup_end = .);
    }

    /* kernel area */
    . = KERNEL_VMA_BASE + ADDR(.text.startup) + SIZEOF(.text.startup) - ADDR(.trampoline);

    .text : AT(ADDR(.text) - KERNEL_VMA_TO_LOAD)
    {
        PROVIDE_HIDDEN(___text_start = .);
        *(.text .text.*)
        PROVIDE_HIDDEN(__text_end = .);
    }

    . = ALIGN(4096);
    .rodata : AT(ADDR(.rodata) - KERNEL_VMA_TO_LOAD)
    {
        PROVIDE_HIDDEN(__rodata_start = .);
        *(.rodata .rodata.*)
        PROVIDE_HIDDEN(__rodata_end = .);
    }

    . = ALIGN(4096);
    .data : AT(ADDR(.data) - KERNEL_VMA_TO_LOAD)
    {
        PROVIDE_HIDDEN(__data_start = .);
        *(.data .data.*)
        PROVIDE_HIDDEN(__data_end = .);

        . = ALIGN(16);
        PROVIDE(__init_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array .ctors))
        PROVIDE(__init_array_end = .);

        . = ALIGN(16);
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
        KEEP(*(.fini_array .dtors))
        PROVIDE_HIDDEN(__fini_array_end = .);

        . = ALIGN(16);
        KEEP(*(SORT_NONE(.init)))

        . = ALIGN(16);
        KEEP(*(SORT_NONE(.fini)))

        . = ALIGN(16);
        KEEP(*(.ctors))

        . = ALIGN(16);
        KEEP(*(.dtors))
    }

    . = ALIGN(4096);
    .bss : AT(ADDR(.bss) - KERNEL_VMA_TO_LOAD)
    {
        PROVIDE_HIDDEN(___bss_start = .);
        *(.bss .bss.*)
        PROVIDE_HIDDEN(___bss_end = .);
    }

    PROVIDE_HIDDEN(__end = .);
}
