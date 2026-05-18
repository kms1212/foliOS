# Common {#common}

Common headers describe shared data structures and boot protocol interfaces used
across bootloader and kernel boundaries.

The generated Common reference is intentionally separate from Strata and Vellum
so shared protocol types do not obscure either component's API index.

## `strata`

The `strata` headers describe the normalized bootloader-module-to-kernel ABI.
The `load_folios` Vellum module gathers data from the initialized bootloader
environment, builds the boot information table, and hands that table to Strata.

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
and do not expose Vellum-core-private discovery state through this boundary.
