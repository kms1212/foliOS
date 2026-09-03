# Guard Pages {#strata_memory_guard_pages}

Guard pages are represented as mapping policy and reservation metadata.

## Fixed Guard

`MF_GUARD` reserves one inaccessible page adjacent to the usable range. The VMM
reservation stores the guard count so range queries and frees can recover the
whole reservation from a usable page.

Guard pages are not caller-visible usable memory. They are part of the VMM
reservation and should be released with the reservation they protect.

## Grow-Down Guard

`MF_GUARD_GROW_DOWN` is a subpolicy of `MF_GUARD`. It is used for local
downward-growing stacks and has stricter rules:

- it requires `MF_GUARD`;
- it is local-only;
- it cannot be combined with `MF_IMMEDIATE`;
- it requires a demand-zero mapping policy.

When a fault lands exactly on the guard page below the usable stack range, VMM
can extend the reservation downward by one page if there is space and the next
lower page is unmapped.

## Stack Use

User stacks are allocated near the top of the user range with
`MF_USER_DEFAULT | MF_GUARD | MF_GUARD_GROW_DOWN`. The initial usable range is
small, and pages are materialized by demand paging. The guard page keeps one
inaccessible page below the current stack boundary.

Kernel stacks currently use fixed guard pages and are allocated in a global
kernel domain.
