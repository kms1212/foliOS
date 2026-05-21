# amd64-pc-bios {#architecture_target_amd64_pc_bios}

`amd64-pc-bios` is the main development profile for running AMD64 Strata on a
PC-compatible BIOS machine. It combines an AMD64 kernel with the IA-32 BIOS
Vellum loader path.

## Composition

- Build target: `amd64-pc-bios`.
- Strata architecture: `amd64`.
- Platform: `pc`.
- Firmware path: `bios`.
- Vellum architecture: `ia32`, because BIOS startup enters the IA-32 loader
  path before the AMD64 kernel takes over.
- Usual QEMU machine script profile: `pc-amd64`.
- Disk image bootloader architecture option: `scripts/mkdisk.sh -a ia32
  disk.img`.

The `-a ia32` disk-image option names the bootloader architecture, not the
kernel architecture.

## Boot And Handoff

Firmware enters the IA-32 PC BIOS Vellum startup path. Vellum initializes the
loader environment, reads boot configuration, and loads the `load_folios`
bootloader module through the `loadmodule` command.

`load_folios` is the current `stload` producer. It collects loader and platform
information, builds the boot information table, loads the AMD64 kernel image,
and transfers control to the kernel entry point.

The current handoff enters the kernel ELF entry point in IA-32 protected mode.
`EDX` points to the `stload` boot information table, paging is already enabled
in a loader-provided 32-bit address space, and the loader page tables are
recursively visible at `0xFFC00000..0xFFFFFFFF`. The detailed register, stack,
table, and required-entry contract is documented in
[stload Handoff ABI](../../common/stload-abi.md).

After handoff, Strata owns the transition from the loader entry profile into
normal AMD64 kernel execution. Loader-private objects, shell state, device
objects, and allocator metadata are not part of the kernel ABI.

## Memory And Address Spaces

The profile uses the shared Strata memory model: global kernel mappings, local
address spaces, explicit reserve/commit/map operations, managed page metadata,
demand paging, and guard-page policies. The boot handoff supplies the physical
memory map and unavailable frame ranges needed to seed PMM and VMM state.

AMD64-specific code is responsible for page-table format, CR3 transitions,
canonical address constraints, interrupt frame layout, syscall entry mechanics,
and the KRT mapping shape. Conceptual memory rules remain in
[Memory Model](../memory-model.md) and [Strata Memory](../../strata/memory/index.md).

- @subpage architecture_target_amd64_la48_memory_map "AMD64 LA48 Memory Map"

## Interrupts And Time

The PC platform layer owns legacy and modern PC interrupt/timer plumbing for
this profile: GDT, IDT, TSS, PIC/APIC setup, PIT/HPET clock setup, interrupt
entry, and syscall frame handoff. Higher-level interrupt ownership and future
direct module IRQ entry are architecture-level runtime contracts rather than
BIOS-specific loader behavior.

## Module Protection Backend

The generic module protection model is expressed in terms of module protection
shards, not MPK as a public kernel concept. On AMD64, the intended backend uses
MPK/PKRU and PCID-aware page-table views to implement shard-local protection
bindings.

Module protection is a fault-containment mechanism for trusted but potentially
buggy modules. It is not the ordinary user/kernel security boundary, and module
code must still go through KRT and the module runtime for valid cross-module
calls or data access. See
[Module Protection Shards and Fault Containment](../../decisions/0006-module-protection-shards-and-fault-containment.md).

## Current Boundaries

- BIOS boot currently depends on the IA-32 Vellum path.
- UEFI and non-PC firmware paths are separate profiles, even when they reuse
  common Vellum or Strata code.
- Module archive loading, the module runtime, and the protection-shard planner
  are active design and implementation areas.
- Any code that consumes boot data should depend on `stload`, not on
  Vellum-private data structures.

## Related Pages

- [Boot Flow](../boot-flow.md)
- [stload Handoff ABI](../../common/stload-abi.md)
- [Disk Images](../../development/disk-images.md)
- [Vellum Boot Flow](../../vellum/boot-flow.md)
