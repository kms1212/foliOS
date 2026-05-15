# VMM {#strata_memory_vmm}

The virtual memory manager owns virtual address reservations and metadata.

## Domains

Global domains are selected by `enum StVmm_Domain`:

- `VMM_DOMAIN_KERNEL_FAST`;
- `VMM_DOMAIN_KERNEL_SLOW`;
- `VMM_DOMAIN_IO`;
- `VMM_DOMAIN_MODULE`;
- `VMM_DOMAIN_KRT_GLOBAL`.

Local domains are stored in `StAddressSpace` and cover the process user range.
They are initialized with `StVmm_InitLocalDomain` and removed with
`StVmm_RemoveLocalDomain`.

## Reservation Nodes

VMM records ranges as reservation nodes. A node stores:

- base and limit virtual page numbers;
- owner link;
- domain/list links;
- allocation flags and mapping flags;
- reservation type (`Allocate`/`Map` at MM level);
- physical layout policy;
- mapping policy;
- guard-page count;
- live/dead state.

The list is sorted by virtual page range. VMM validates list structure in
critical paths so corruption is caught close to the metadata owner.

## Mapping Policy

`struct StVmm_PageMappingPolicy` currently distinguishes:

- physical mappings;
- demand-zero mappings;
- image-backed mappings;
- guard pages.

Page-fault handling asks VMM to resolve the local page first, then fetches page
information and asks MM/platform mapping code to materialize the page when the
policy allows it.

## Owner Links

VMM reservations can be linked to `StAllocationOwner`. Closing an owner walks
those reservations and releases the matching mappings/allocations. This is the
reason owner data belongs on the reservation node and not only in PMM frame
metadata.
