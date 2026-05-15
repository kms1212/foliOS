# Vellum ELF Loading {#vellum_elf_loading}

This page describes how Vellum loads ELF images and constructs module/kernel
handoff metadata.

## ELF API

Vellum owns a small ELF API:

- `VlElf_Open` / `VlElf_Close`;
- header and program-header accessors;
- program loading;
- section lookup/loading;
- symbol lookup.

The API supports both loader work and module metadata discovery. Architecture
details, such as machine type and relocation rules, should stay behind the
architecture-specific ELF boundary where possible.

## Kernel Loading

When loading Strata, Vellum should validate the ELF identity, class,
endianness, machine, and program headers before mapping segments. Loaded ranges
must be reflected in unavailable-frame boot information so Strata does not
reuse memory occupied by the kernel image, page tables, boot assets, or ramdisk.

## Symbol Data

Vellum has historically had ELF symbol-reading code. Strata now also has a
static symbol-info path for panic-time lookup. The intended split is:

- early/panic/interrupt-safe Strata lookup uses static symbol information linked
  into the kernel image;
- richer runtime symbol loading may later use ELF parsing once normal kernel
  services are safe to depend on.

Do not make early panic diagnostics depend on Vellum-only ELF state.
