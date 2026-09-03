# Allocation Owner {#strata_memory_allocation_owner}

`StAllocationOwner` tracks ownership and cleanup for memory reservations and
physical backing.

## Role

`StAllocationOwner` is a first-class object that records responsibility for
allocation and cleanup independently of the memory subtype.

The owner tracks:

- VMM reservations linked to the owner;
- current page usage;
- peak page usage;
- closed/dying state through `StRefControlBlock`.

## Cleanup

Closing an owner marks it closed and walks its VMM reservation list. Allocation
reservations are freed and map reservations are unmapped through the MM public
lifecycle functions. This keeps teardown policy in one place and avoids leaking
memory when process exit races with outstanding ranges.

After the owner is closed, new reservations should reject it. Existing PMM and
VMM records keep strong owner references until they are released.

## Process Integration

Processes own an allocation owner. Local user allocations normally charge the
process owner through the address space/process relationship. This allows
process teardown to reclaim:

- demand-zero pages that were never touched;
- image-backed reservations;
- committed local pages;
- global or local ranges explicitly attached to the owner.

Address spaces do not own allocation cleanup by themselves. They describe the
virtual domain; the allocation owner owns accounting and the teardown walk.
