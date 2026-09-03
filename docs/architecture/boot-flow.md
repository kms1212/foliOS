# Boot Flow {#architecture_boot_flow}

This page describes the firmware-to-kernel flow at the architecture level. See
[Vellum Boot Flow](../vellum/boot-flow.md) for bootloader internals.

## High-Level Sequence

1. Firmware enters the platform-specific Vellum startup path.
2. Vellum initializes enough CPU, memory, disk, filesystem, video, and console
   support to read boot configuration and load bootloader modules.
3. Vellum loads the `load_folios` bootloader module through the `loadmodule`
   command.
4. The `load_folios` module is the current `stload` producer: it collects
   bootloader/platform information, builds the boot information table, loads the
   kernel image, and transfers control to the kernel entry point.
5. Strata consumes the boot information table through the common handoff ABI and
   begins kernel initialization.
6. Once a runnable thread exists, scheduler dispatch becomes the normal
   execution path.

For the current `amd64-pc-bios` target, the bootloader architecture is still
IA-32 even though the kernel target is AMD64. This is why disk-image generation
uses `scripts/mkdisk.sh -a ia32 disk.img`.

## `stload` Handoff

Vellum loads `load_folios` as a bootloader module, keeping its policy outside
Vellum core. Vellum first initializes the environment needed to load modules.
The current loader-side handoff policy belongs to `load_folios`. The module
requests the required information, normalizes it into the boot information
table, and enters the kernel through the `stload` ABI.

The shared `stload` headers describe the ABI of that table. Important entries
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

The detailed contract is documented in
[stload Handoff ABI](../common/stload-abi.md). Consumers should use that
contract rather than Vellum-private loader state. Whether a value came from
firmware probing, a Vellum device interface, a configuration file, or another
bootloader module is hidden behind the producer boundary.

## Initialization Boundary

Before the scheduler is live, code paths must be conservative: allocation
sources are limited, panic reporting must not depend on fully initialized
services, and symbol resolution should use static data. After the scheduler and
process model are active, richer services become available, but early and
panic paths still need to keep working without them.
