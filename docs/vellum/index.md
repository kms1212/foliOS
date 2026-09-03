# Vellum {#vellum}

Vellum documents describe the bootloader core, platform setup, and module
runtime used before kernel handoff.

- @subpage vellum_boot_flow "Boot Flow"
- @subpage vellum_memory "Memory"
- @subpage vellum_elf_loading "ELF Loading"
- @subpage vellum_modules "Modules"

## Scope

Vellum initializes the platform services required to run bootloader commands
and modules. Producer modules control kernel image loading and handoff policy;
Vellum core provides the supporting environment.

Vellum is undergoing an API migration. Some public functions and types still
use older lowercase naming such as `mm_map`, `vpn_t`, and `struct module`. New
or touched code should move toward the shared foliOS style without creating
large mechanical churn.
