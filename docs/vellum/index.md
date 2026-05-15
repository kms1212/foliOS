# Vellum {#vellum}

Vellum documents describe the bootloader and its handoff contract with Strata.

- @subpage vellum_boot_flow "Boot Flow"
- @subpage vellum_memory "Memory"
- @subpage vellum_elf_loading "ELF Loading"
- @subpage vellum_modules "Modules"

## Scope

Vellum is responsible for doing enough platform work to load Strata and describe
the machine in a stable handoff format. It is not the long-term owner of kernel
policy. Once Strata starts, memory ownership, process/thread lifetime, and
runtime services move to the kernel.

Vellum is also a migration-in-progress component. Some public functions and
types still use older lowercase naming such as `mm_map`, `vpn_t`, and
`struct module`. New or touched code should move toward the shared foliOS style
without creating large mechanical churn.
