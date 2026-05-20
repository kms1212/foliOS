# Vellum {#vellum}

Vellum documents describe the bootloader core, platform setup, and module
runtime used before kernel handoff.

- @subpage vellum_boot_flow "Boot Flow"
- @subpage vellum_memory "Memory"
- @subpage vellum_elf_loading "ELF Loading"
- @subpage vellum_modules "Modules"

## Scope

Vellum is responsible for doing enough platform work to run bootloader commands
and modules. Kernel image loading and handoff policy belong to producer modules,
not to Vellum core.

Vellum is also a migration-in-progress component. Some public functions and
types still use older lowercase naming such as `mm_map`, `vpn_t`, and
`struct module`. New or touched code should move toward the shared foliOS style
without creating large mechanical churn.
