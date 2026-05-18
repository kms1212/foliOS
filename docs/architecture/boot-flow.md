# Boot Flow {#architecture_boot_flow}

This page describes the firmware-to-kernel flow at the architecture level. See
[Vellum Boot Flow](../vellum/boot-flow.md) for bootloader internals.

## High-Level Sequence

1. Firmware enters the platform-specific Vellum startup path.
2. Vellum initializes enough CPU, memory, disk, filesystem, video, and console
   support to read boot configuration and load bootloader modules.
3. Vellum loads the `load_folios` bootloader module through the `loadmodule` command.
4. The `load_folios` module collects bootloader/platform information, builds the
   boot information table, loads or finalizes the kernel handoff, and transfers
   control to Strata.
5. Strata receives the boot information table through the common handoff ABI and
   initializes platform state, memory management, interrupts, timers,
   process/thread infrastructure, and diagnostics.
6. Once a runnable thread exists, scheduler dispatch becomes the normal
   execution path.

For the current `amd64-pc-bios` target, the bootloader architecture is still
IA-32 even though the kernel target is AMD64. This is why disk-image generation
uses `scripts/mkdisk.sh -a ia32 disk.img`.

## Strata Load Handoff

`load_folios` is a Vellum bootloader module, not a pile of code that the Vellum core
executes inline. Vellum first brings up the environment needed to load modules.
After that, `load_folios` owns the kernel handoff policy: it asks the bootloader for
the information it needs, normalizes that information into the bootinfo table,
and passes the table to Strata.

The shared `strata` headers describe the ABI of that table. Important entries
include:

- command arguments;
- loader identity;
- memory map;
- system disk metadata;
- ACPI RSDP;
- framebuffer and default font;
- unavailable physical frame ranges;
- boot page-table information;
- RAM disk location.

Strata should treat this table as a compact boot contract, not as Vellum core
state. Whether the value came from firmware probing, a Vellum device interface,
a configuration file, or another bootloader module should be hidden behind the
`load_folios` handoff.

## Initialization Boundary

Before the scheduler is live, code paths must be conservative: allocation
sources are limited, panic reporting must not depend on fully initialized
services, and symbol resolution should use static data. After the scheduler and
process model are active, richer services can be layered on top, but early and
panic paths still need to keep working without them.
