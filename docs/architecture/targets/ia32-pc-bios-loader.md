# IA-32 PC BIOS Loader {#architecture_target_ia32_pc_bios_loader}

The IA-32 PC BIOS loader profile describes Vellum's role in the current BIOS
boot process. Strata's active kernel architecture is AMD64, while this profile
covers only the loader architecture used by `amd64-pc-bios` to reach the AMD64
kernel.

## Role

The loader profile covers firmware entry, early CPU setup, BIOS services,
disk/filesystem access, console setup, boot configuration, and bootloader module
execution. It initializes enough of the environment for modules such as
`load_folios` to perform policy work.

The current kernel handoff delegates table construction to `load_folios` rather
than Vellum core. Vellum loads the module, which then becomes the `stload`
producer.

## Firmware And Platform Constraints

BIOS boot starts from IA-32-compatible firmware state and uses PC platform
conventions. Loader code must account for low-memory constraints, firmware
interrupt services, legacy disk paths, and the fact that the final kernel may
not share the loader architecture.

The current `amd64-pc-bios` handoff still enters the kernel in IA-32 protected
mode. The loader-provided page tables must be sufficient to execute the kernel
entry code and read the boot information table before Strata switches into its
normal AMD64 environment.

## Handoff-Owned Data

Only normalized ABI data crosses the Vellum-to-Strata boundary. Loader
allocations that must survive handoff are represented by `stload` entries,
especially unavailable frame ranges and the boot information table itself.

The current profile also exposes the active loader page-directory location
through `BET_PAGETABLE_VPN`, with the 32-bit recursive mapping visible at
`0xFFC00000..0xFFFFFFFF`. Consumers should treat this as part of the documented
handoff profile, not as permission to depend on arbitrary Vellum internals.

## Scope Limits

- Vellum device objects, shell objects, filesystem handles, and allocator state
  are loader-private.
- BIOS-specific details should stay behind Vellum platform code or the `stload`
  producer boundary.
- Kernel code should consume the packed `stload` table and shared headers
  instead of inspecting loader implementation details.
- A future non-BIOS profile may keep the same high-level boot flow while using a
  different firmware path, entry state, or page-table handoff.

## Related Pages

- [Vellum Boot Flow](../../vellum/boot-flow.md)
- [Boot Flow](../boot-flow.md)
- [stload Handoff ABI](../../common/stload-abi.md)
- [Disk Images](../../development/disk-images.md)
