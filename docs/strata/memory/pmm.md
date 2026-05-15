# PMM {#strata_memory_pmm}

The physical memory manager owns physical frame allocation and PMM metadata.

## Responsibilities

PMM owns:

- total/free physical frame accounting;
- contiguous frame allocation;
- frame usability marking during boot and platform discovery;
- allocation metadata lookup;
- ownership references for allocated frames.

Higher layers should not modify PMM metadata directly. They request frames
through `StPmm_AllocateContiguousFrame` and return them through
`StPmm_FreeContiguousFrame`.

## Ownership Metadata

Each allocated run has `struct StPmm_AllocationMetadata` with:

- allocation order;
- flags;
- `StAllocationOwner_StrongRef owner`.

The owner reference ties physical backing to the same cleanup/accounting model
used by VMM reservations. If a process or subsystem owns memory, that ownership
should be reflected in PMM metadata for as long as the backing exists.

## Metadata Access

PMM exposes two views:

- `StPmm_GetAllocMetadata` returns a borrowed metadata view.
- `StPmm_LockAndGetAllocMetadata` returns a locked metadata view that must be
  released with `StPmm_UnlockAllocMetadata`.

The locked form exists because metadata can be inspected while the allocator is
mutating state. Use the reference typedefs to make that distinction visible at
the call site.

## Higher-Layer Contract

MM/VMM should decide virtual placement and mapping policy. PMM should only know
about physical placement constraints such as `AF_PMM_BELOW_*` and allocation
ownership. Do not push page-fault policy, guard policy, or user/kernel virtual
layout into PMM.
