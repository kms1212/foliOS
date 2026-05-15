# Vellum Memory {#vellum_memory}

This page describes boot-time memory discovery, temporary mappings, allocation,
and the memory map passed to Strata.

## Physical Memory Allocator

The Vellum PMA tracks physical frame availability during boot:

- `mm_pma_init` initializes the usable inclusive physical range.
- `mm_pma_mark_reserved` marks inclusive firmware, loader, kernel, module, or
  asset ranges unavailable.
- `mm_pma_allocate_frame` and `mm_pma_free_frame` provide frame runs for loader
  needs.

This allocator exists so Vellum can load the kernel and build handoff data. It
does not replace Strata PMM after the kernel starts.

## Virtual Mapping

Vellum uses page-level helpers:

- `mm_map`;
- `mm_unmap`;
- `mm_allocate_pages`;
- `mm_allocate_pages_to`;
- `mm_vpn_to_pfn`;
- `mm_vaddr_to_paddr`.

Mapping flags are simple bootloader flags such as `PF_READONLY`, `PF_USER`, and
cache policy bits. Do not confuse them with Strata `MF_*` flags; the two domains
belong to different components.

## Handoff Memory Map

Vellum should expose enough memory information for the `loadst` module to build
normalized memory-map and unavailable-frame boot information entries. Strata
consumes that table to initialize its own PMM and VMM state.
