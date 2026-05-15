# Reserve, Commit, and Map {#strata_memory_reserve_commit_map}

`Reserve`, `Commit`, and `Map` are separate operations.

`Reserve` records a virtual address range and its policy without requiring
present pages. `Commit` materializes backing for a reserved range. `Map`
attaches an existing physical frame range and is paired with `Unmap`.

`Free` releases memory acquired through `Allocate` or `Reserve`, including
reserved ranges that were never committed.

## Why The Split Exists

At MM level, "allocate memory" can mean several different things:

- choose a virtual address range;
- charge an owner;
- allocate physical frames;
- install present page-table entries;
- record a lazy policy for future page faults.

Keeping those steps separate gives the API vocabulary enough precision:

- `Reserve` means virtual address space and policy are recorded.
- `Commit` means physical backing is attached to a previous reservation.
- `Map` means the caller already owns the physical frame range being attached.
- `Allocate` is the high-level convenience path that reserves and acquires
  backing according to flags.

## Public Lifecycles

`Map -> Unmap` is for caller-owned physical frames:

```c
StStatus StMm_MapLocal(...);
void StMm_UnmapLocal(...);
```

`Allocate -> Free` is for memory acquired through PMM:

```c
StStatus StMm_AllocateLocalSparse(...);
void StMm_FreeLocal(...);
```

`Reserve -> Commit -> Free` is for virtual address space that becomes backed
later:

```c
StStatus StMm_ReserveLocalSparse(...);
StStatus StMm_CommitLocal(...);
void StMm_FreeLocal(...);
```

`Free` is `void` because the useful caller policy is already over by the time
the kernel is tearing down memory. If it finds an impossible state, it should
assert or panic rather than hand a mostly-unrecoverable error to the caller.

## Lazy Local Allocations

Non-immediate local sparse allocations may be represented internally as
on-demand reservations. The public caller still asked to allocate usable memory;
the implementation chooses to materialize physical frames on first access. This
is why `Reserve` is not synonymous with demand paging, and why demand policy is
recorded in VMM metadata rather than exposed as a separate public allocator.
