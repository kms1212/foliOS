cmake_minimum_required(VERSION 3.13)

target_sources(strata_temp_module_acpi PRIVATE event.c interrupt.c io.c log.c mem.c misc.c mutex.c pci.c rsdp.c spinlock.c work.c)
