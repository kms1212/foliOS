# Strata Memory {#strata_memory}

The Strata memory documentation covers PMM, VMM, MM API layering, demand
paging, guard pages, and allocation ownership.

- @subpage strata_memory_api_model "API Model"
- @subpage strata_memory_reserve_commit_map "Reserve, Commit, and Map"
- @subpage strata_memory_pmm "PMM"
- @subpage strata_memory_vmm "VMM"
- @subpage strata_memory_demand_paging "Demand Paging"
- @subpage strata_memory_guard_pages "Guard Pages"
- @subpage strata_memory_allocation_owner "Allocation Owner"

## Layering

The memory stack is intentionally layered:

- PMM owns physical frames and frame metadata.
- VMM owns virtual address reservations and mapping policy metadata.
- MM is the public lifecycle layer that combines PMM, VMM, and platform page
  table hooks.
- `StAddressSpace` owns a local user VMM domain.
- `StAllocationOwner` owns accounting and cleanup for memory charged to a
  process or kernel subsystem.

Callers should normally use the public `StMm_*` API rather than reaching into
VMM or PMM directly. VMM and PMM remain available to internal kernel subsystems
through lower-level contracts.
