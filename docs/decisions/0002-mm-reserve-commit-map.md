# 0002: MM Reserve, Commit, and Map {#decision_0002_mm_reserve_commit_map}

Status: accepted

Date: 2026-05-15

## Context

The MM layer needs to distinguish three related operations:

- reserving virtual address space and policy;
- committing physical backing to a reserved range;
- mapping caller-owned physical frames.

## Decision

Use three public lifecycle families:

- `Map -> Unmap`;
- `Allocate -> Free`;
- `Reserve -> Commit -> Free`.

`Allocate` remains the high-level operation that returns usable memory. It may
materialize backing immediately or use an internal on-demand policy. Public
`Reserve` does not imply demand paging by itself.

## Consequences

The VMM records reservations and policy. The MM layer exposes the public
lifecycle and performs PMM allocation and platform mapping.

This keeps demand paging an implementation policy of selected MM/VMM paths
rather than a meaning smuggled into every public `Reserve` call.

## Related Docs

- [Memory API Model](../strata/memory/api-model.md)
- [Reserve, Commit, and Map](../strata/memory/reserve-commit-map.md)
- [Demand Paging](../strata/memory/demand-paging.md)
