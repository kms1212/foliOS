# stload Handoff ABI {#common_stload_abi}

`stload` is the normalized bootloader-module-to-kernel handoff ABI used between
Vellum's `load_folios` module and Strata. It is the boundary where platform
discovery, boot media policy, and loader-private state are converted into packed
common structures that the kernel can consume without depending on Vellum
internals.

This page describes the current ABI version, `BTV_CURRENT == 0`, as implemented
by the `amd64-pc-bios` path. The data ABI is intended to survive alternate
loaders such as a future QEMU direct-boot loader; the current IA-32 entry-state
details are a platform compatibility contract, not the desired long-term shape
of every loader.

## Ownership

Vellum owns firmware and boot-media interaction before handoff. The
`load_folios` module owns Strata load policy: it loads the kernel image, builds
the boot information table, reserves the frames that must survive early kernel
initialization, and transfers control to the kernel entry point.

Strata owns everything after handoff. It must treat the boot information table
as a compact contract, not as Vellum core state. Strata code should not inspect
Vellum device objects, filesystem objects, shell state, or allocator metadata.

The shared `common/include/stload` headers are the source of truth for packed
structure layout. Conceptual rules that are not expressible in C layout live in
this document.

## Control Transfer

The handoff is one-way. The loader must not expect the kernel to return, and
the kernel may reuse loader-owned memory after it has copied or reserved the
handoff data it needs.

For the current `amd64-pc-bios` path, Vellum enters the Strata ELF entry point
in IA-32 protected mode. The entry state is:

- `EDX` contains a pointer to `struct StLoad_BootInfoTableHeader`.
- General-purpose registers other than `EDX` are unspecified.
- `ESP` points into loader-provided scratch stack memory. At least 16 KiB of
  writable, downward-growing stack space must be available below `ESP`; stack
  contents are unspecified.
- `EFLAGS.IF` must be clear at entry. Loaders must not rely on maskable
  interrupt delivery after transfer, and should leave legacy interrupt
  controllers masked or quiesced when they previously enabled them.
- `EFLAGS.DF` must be clear at entry.
- Paging is enabled in a loader-provided 32-bit address space sufficient to
  execute the linked Strata startup window and access the boot information
  table.
- The current amd64 trampoline expects the active 32-bit page directory and page
  tables to be recursively visible at `0xFFC00000..0xFFFFFFFF`.

The recursive page-table dependency is an implementation compatibility detail of
the current Strata trampoline. A cleaner future handoff should replace it with
an explicit amd64 handoff page table or a direct 64-bit entry contract while
keeping the `stload` boot information table unchanged.

## Kernel Image Loading

For executable kernel images, ELF program-header `p_paddr` values are the
loader-visible load addresses. They must name addresses that the active loader
can map and write before handoff. Kernel virtual addresses may differ from load
addresses, but that relationship must be expressed by the kernel image itself,
not by truncating or otherwise reinterpreting `p_paddr` in the loader.

The current `amd64-pc-bios` Strata image starts in the low handoff window at
`0xC0000000` and later maps the high-half kernel at its linked virtual address.
Its ELF `p_vaddr` values may therefore be high canonical addresses, while
corresponding `p_paddr` values remain low loader addresses.

## Table Layout

The boot information table begins with `StLoad_BootInfoTableHeader`. The string
table starts immediately after the fixed header and occupies
`header_size - sizeof(struct StLoad_BootInfoTableHeader)` bytes. Boot
information entries start at `table + header_size` and are walked by repeatedly
adding each entry header's `size`.

All `stload` structures are packed. Producers must zero reserved fields.
Producers should align the table header and each entry payload to 16 bytes.
Consumers must use the `header_size` and `size` fields rather than assuming C
structure padding.

The table is little-endian when `BTF_BIGENDIAN` is clear. Big-endian tables are
reserved by the flag definition but are not part of the current Strata boot
path.

String references are byte offsets into `table->strtab`. Strings are
NUL-terminated. Offsets must point inside the string table region.

Entry types are unique in ABI version 0 unless an entry definition explicitly
allows multiplicity. Producers must not emit duplicate v0 entries.

Unknown optional entries may be ignored. Unknown entries marked `BEF_REQUIRED`
make the table unusable for a consumer that does not understand them.

## Address Semantics

Address fields in entry payloads are physical addresses unless an entry
explicitly says otherwise. Important exceptions and conventions are:

- The boot information table pointer passed in `EDX` is a handoff virtual
  address. It is only guaranteed to be valid in the initial handoff address
  space until Strata copies the table.
- `BET_PAGETABLE_VPN` contains a loader virtual page number for the loader
  page-directory structure described by the platform handoff, not a physical
  address. Current Strata initialization validates this entry but does not use
  the payload after the startup trampoline has already run.
- `BET_COMMAND_ARGS` and `BET_LOADER_INFO` string values are string-table
  offsets.
- `BET_RAMDISK` extents use physical addresses and byte sizes.
- `BET_FRAMEBUFFER.framebuffer_addr` is the physical or MMIO base address of
  the framebuffer described by the entry.

If a payload points to memory that must remain available after the physical
memory allocator starts, the producer must also describe the backing frames in
`BET_UNAVAILABLE_FRAMES`.

## Required Entries

Current Strata early initialization requires these entries:

- `BET_MEMORY_MAP`: physical memory availability.
- `BET_UNAVAILABLE_FRAMES`: loader-created frame reservations.
- `BET_PAGETABLE_VPN`: current IA-32 loader page-directory virtual location.

Current Strata bring-up also expects:

- `BET_FRAMEBUFFER`: text-mode framebuffer information for early display.
- `BET_ACPI_RSDP`: ACPI discovery root for the platform module path.
- `BET_RAMDISK`: boot RAM disk containing the configured SystemManager image.

The current Vellum `load_folios` producer treats the RAM disk as mandatory:
`-ramdisk path` is required, and the producer always emits a required
`BET_RAMDISK` entry.

`BET_COMMAND_ARGS` is operationally optional. If it is absent, Strata uses
built-in defaults for options such as the SystemManager path. The current Vellum
producer always emits it, even when the argument count is zero.

`BET_LOADER_INFO` is informational and should identify the producer. The current
Vellum producer emits it as required, but Strata must not depend on Vellum as
the only possible producer.

`BET_SYSTEM_DISK`, `BET_DEFAULT_FONT`, and `BET_BOOT_GRAPHICS` are ABI-reserved
for the existing structure definitions but are not required by the current
Strata startup path.

## Memory Map

`BET_MEMORY_MAP` describes firmware-level physical memory regions. Types mirror
the normalized `BEMT_*` constants:

- `BEMT_FREE`: usable RAM.
- `BEMT_RESERVED`: unavailable or firmware-reserved memory.
- `BEMT_ACPI_RECLAIMABLE`: ACPI reclaimable memory.
- `BEMT_ACPI_NVS`: ACPI non-volatile sleep memory.
- `BEMT_BAD`: defective memory.

The kernel first marks free ranges from this entry, then marks non-free ranges
unusable. Loader-created allocations inside otherwise free memory must therefore
also appear in `BET_UNAVAILABLE_FRAMES`.

## Unavailable Frames

`BET_UNAVAILABLE_FRAMES` contains physical frame ranges that the kernel must not
allocate as ordinary free memory during early initialization. Each entry is
`pfn_base`, `count`, and a `BEUT_*` reason.

Current reasons are:

- `BEUT_PAGETABLE`: loader page tables used by the current handoff path.
- `BEUT_KERNEL`: loaded kernel image frames.
- `BEUT_DEFAULT_FONT`: default font backing frames.
- `BEUT_BOOT_GRAPHICS`: boot graphics backing frames.
- `BEUT_RAMDISK`: boot RAM disk backing frames.
- `BEUT_BOOTINFO`: boot information table backing frames.

`BEUT_PAGETABLE` is special in the current Strata path: after the trampoline has
migrated to kernel-owned page tables, the physical memory allocator may release
those frames. Other reasons describe data that must remain reserved until the
kernel explicitly copies, maps, or retires it.

## RAM Disk

`BET_RAMDISK` describes a boot RAM disk as one or more physical extents. The
logical image is reconstructed by copying extents in order until `size` bytes
have been materialized. Extents may be smaller than a page at the end of the
image.

The image format starts with `StLoad_RamdiskHeader`. Directory entries are
`StLoad_RamdiskDirEntry` records with 4-byte alignment. File and directory
payload offsets are byte offsets from the start of the RAM disk image.

The RAM disk ABI is deliberately loader-neutral. Vellum currently builds it from
a required boot filesystem directory, while a future direct-boot loader may
receive an already-built image from a host tool.

## Evolution Rules

ABI changes should be conservative:

- Add optional entries for new data when possible.
- Do not reuse entry type values or reason values.
- Keep reserved fields zero on write and ignored on read.
- Bump the table version for incompatible layout or semantic changes.
- Document any new platform entry-state requirement separately from the packed
  boot information table.

The next cleanup target is to split the current amd64 trampoline dependency out
of the data ABI. A future `amd64` handoff contract should define either a
minimal loader-provided long-mode page table or a direct 64-bit entry state, so
Vellum and any QEMU direct-boot loader can share the same `stload` table without
sharing Vellum's transient IA-32 address space.
