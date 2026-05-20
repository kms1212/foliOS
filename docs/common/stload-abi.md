# stload Handoff ABI {#common_stload_abi}

`stload` is the normalized loader-to-kernel handoff ABI. It is the boundary
where platform discovery, boot media policy, and loader-private state are
converted into packed common structures that a kernel can consume without
depending on loader internals.

This page describes the current ABI version, `BTV_CURRENT == 0`, and the
current `amd64-pc-bios` entry-state profile. The packed boot information table
and the CPU entry-state profile are separate parts of the handoff contract.

## Ownership

The producer owns firmware and boot-media interaction before handoff. It loads
the kernel image, builds the boot information table, reserves the frames that
must survive handoff, and transfers control to the kernel entry point.

The consumer owns execution after handoff. It must treat the boot information
table as ABI data, not as producer-private state. Consumers must not inspect
producer device objects, filesystem objects, shell state, or allocator metadata.

The shared `common/include/stload` headers are the source of truth for packed
structure layout. Conceptual rules that are not expressible in C layout live in
this document.

## Control Transfer

The handoff is one-way. The loader must not expect the kernel to return, and
the kernel may reuse loader-owned memory after it has copied or reserved the
handoff data it needs.

For the current `amd64-pc-bios` profile, the producer enters the kernel ELF
entry point in IA-32 protected mode. The entry state is:

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
  execute the kernel entry code and access the boot information table.
- The active 32-bit page directory and page tables must be recursively visible
  at `0xFFC00000..0xFFFFFFFF`.

## Kernel Image Loading

For executable kernel images, ELF program-header `p_paddr` values are the
loader-visible load addresses. They must name addresses that the active loader
can map and write before handoff. Kernel virtual addresses may differ from load
addresses, but that relationship must be expressed by the kernel image itself,
not by truncating or otherwise reinterpreting `p_paddr` in the loader.

A high-half kernel may therefore expose high canonical `p_vaddr` values while
using low loader-visible `p_paddr` values for the bytes copied before handoff.

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
reserved by the flag definition but are not part of ABI version 0.

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
  space.
- `BET_PAGETABLE_VPN` contains a loader virtual page number for the loader
  page-directory structure described by the platform handoff, not a physical
  address.
- `BET_COMMAND_ARGS` and `BET_LOADER_INFO` string values are string-table
  offsets.
- `BET_RAMDISK` extents use physical addresses and byte sizes.
- `BET_FRAMEBUFFER.framebuffer_addr` is the physical or MMIO base address of
  the framebuffer described by the entry.

If a payload points to memory that must remain available after the physical
memory allocator starts, the producer must also describe the backing frames in
`BET_UNAVAILABLE_FRAMES`.

## Required Entries

The current `amd64-pc-bios` profile requires these entries:

- `BET_MEMORY_MAP`: physical memory availability.
- `BET_UNAVAILABLE_FRAMES`: loader-created frame reservations.
- `BET_PAGETABLE_VPN`: IA-32 loader page-directory virtual location.
- `BET_FRAMEBUFFER`: framebuffer information.
- `BET_ACPI_RSDP`: ACPI discovery root.
- `BET_RAMDISK`: boot RAM disk image.

The current `load_folios` producer treats the RAM disk as mandatory:
`-ramdisk path` is required, and it always emits a required `BET_RAMDISK`
entry.

`BET_COMMAND_ARGS` carries producer-supplied kernel arguments. Producers may
emit an empty entry when no arguments are provided.

`BET_LOADER_INFO` is informational and should identify the producer. Consumers
must not require a specific producer name.

`BET_SYSTEM_DISK`, `BET_DEFAULT_FONT`, and `BET_BOOT_GRAPHICS` are ABI-reserved
for the existing structure definitions but are not required by the current
`amd64-pc-bios` profile.

## Memory Map

`BET_MEMORY_MAP` describes firmware-level physical memory regions. Types mirror
the normalized `BEMT_*` constants:

- `BEMT_FREE`: usable RAM.
- `BEMT_RESERVED`: unavailable or firmware-reserved memory.
- `BEMT_ACPI_RECLAIMABLE`: ACPI reclaimable memory.
- `BEMT_ACPI_NVS`: ACPI non-volatile sleep memory.
- `BEMT_BAD`: defective memory.

`BEMT_FREE` ranges are allocatable after handoff except for frames also
described by `BET_UNAVAILABLE_FRAMES`. Producer-created allocations inside
otherwise free memory must therefore appear in `BET_UNAVAILABLE_FRAMES`.

## Unavailable Frames

`BET_UNAVAILABLE_FRAMES` contains physical frame ranges that the consumer must
not treat as ordinary free memory immediately after handoff. Each entry is
`pfn_base`, `count`, and a `BEUT_*` reason.

Current reasons are:

- `BEUT_PAGETABLE`: handoff page-table frames.
- `BEUT_KERNEL`: loaded kernel image frames.
- `BEUT_DEFAULT_FONT`: default font backing frames.
- `BEUT_BOOT_GRAPHICS`: boot graphics backing frames.
- `BEUT_RAMDISK`: boot RAM disk backing frames.
- `BEUT_BOOTINFO`: boot information table backing frames.

`BEUT_PAGETABLE` describes handoff page-table frames. A consumer may reclaim
those frames only after it no longer executes under or inspects those tables.
Other reasons describe data that must remain reserved until the consumer
explicitly copies, maps, or retires it.

## RAM Disk

`BET_RAMDISK` describes a boot RAM disk as one or more physical extents. The
logical image is reconstructed by copying extents in order until `size` bytes
have been materialized. Extents may be smaller than a page at the end of the
image.

The image format starts with `StLoad_RamdiskHeader`. Directory entries are
`StLoad_RamdiskDirEntry` records with 4-byte alignment. File and directory
payload offsets are byte offsets from the start of the RAM disk image.

The RAM disk ABI is deliberately loader-neutral. The producer may build the
image from a boot filesystem directory or receive an already-built image from
another source.

## Evolution Rules

ABI changes should be conservative:

- Add optional entries for new data when possible.
- Do not reuse entry type values or reason values.
- Keep reserved fields zero on write and ignored on read.
- Bump the table version for incompatible layout or semantic changes.
- Document any new platform entry-state requirement separately from the packed
  boot information table.

Platform entry-state profiles may evolve independently from the packed boot
information table, as long as each profile documents its register, stack,
interrupt, and address-space requirements explicitly.
