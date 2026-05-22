# Common {#common}

Common headers describe shared data structures and boot protocol interfaces used
across loader and kernel boundaries.

The generated Common reference is intentionally separate from Strata and Vellum
so shared protocol types do not obscure either component's API index.

## `stload`

The `stload` headers describe the normalized loader-to-kernel ABI. The current
`load_folios` module is one producer of that ABI: it gathers data from the
initialized bootloader environment, builds the boot information table, and
enters the kernel through the documented entry-state profile.

See @subpage common_stload_abi for the current handoff contract, including
control-transfer state, table layout rules, address semantics, required entries,
and evolution policy.

Key boot information entries include:

- command arguments;
- loader identity;
- memory map;
- system disk metadata;
- ACPI RSDP;
- framebuffer and default font;
- boot graphics;
- unavailable frame ranges;
- page-table virtual page number;
- RAM disk location.

These structures are packed ABI data. Keep changes conservative and versioned,
and do not expose producer-private discovery state through this boundary.

## Registries

- @subpage common_guids "GUID Registry"
