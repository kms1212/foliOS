# Memory API Model {#strata_memory_api_model}

The public MM API is organized by lifecycle family, address-space scope,
placement, and physical layout.

## Lifecycles

- `Map -> Unmap`: attach caller-provided physical frames to virtual memory.
- `Allocate -> Free`: acquire virtual memory and physical backing together.
- `Reserve -> Commit -> Free`: reserve virtual memory first, then attach
  physical backing later.

## Axes

- `Global` operates in a global VMM domain.
- `Local` operates in a process address space.
- no `To` suffix asks the VMM to choose a virtual address.
- `To` reserves or allocates at a caller-provided virtual address.
- `Contiguous` requires one contiguous physical allocation.
- `Sparse` permits the implementation to use multiple physical runs.

## Naming Grid

The API surface is easiest to read as a grid:

```text
StMm_Map(Global|Local)(To?)
StMm_Allocate(Global|Local)(Contiguous|Sparse)(To?)
StMm_Reserve(Global|Local)(Contiguous|Sparse)(To?)
StMm_Commit(Global|Local)
StMm_Free(Global|Local)
StMm_Unmap(Global|Local)
```

`Map` and `Allocate` both return usable mappings, but they differ in ownership:
`Map` attaches a physical frame range supplied by the caller; `Allocate`
acquires physical backing through PMM. `Reserve` records virtual address space
and policy; `Commit` later materializes physical backing for that reservation.

## Flags

Allocation flags describe placement and alignment:

- `AF_PMM_BELOW_*`: physical placement constraint.
- `AF_ALIGN(a)`: virtual/physical alignment policy.
- `AF_VMM_ALLOC_TOPDOWN`: ask VMM to search from high addresses downward.

Mapping flags describe protection and mapping behavior:

- `MF_WRITABLE`, `MF_USER`, `MF_NO_EXECUTE`, `MF_GLOBAL`;
- cache policy bits such as `MF_NO_CACHE`;
- `MF_IMMEDIATE` for eager materialization;
- `MF_ZERO_FILL` for zero-filled pages;
- `MF_GUARD` and `MF_GUARD_GROW_DOWN` for guard policy.

Keep `StMm_AllocFlags` and `StMm_MapFlags` separate. They are both fixed-width
`__nocast` domains for a reason.

## Ownership

Global allocations and mappings take an explicit `StAllocationOwner_StrongRef`.
Local allocations normally derive ownership from the owning process/address
space. Owner links allow cleanup to be driven by process teardown instead of
requiring every caller to remember every outstanding page range.
