# Demand Paging {#strata_memory_demand_paging}

Demand paging materializes local memory on access when a reservation explicitly
uses an on-demand mapping policy.

## Supported Policies

The current demand path handles local not-present faults for:

- demand-zero pages;
- image-backed pages.

Demand-zero pages allocate a frame and zero it before mapping. Image-backed
pages allocate a frame, zero it, and copy the overlapping content described by
`struct StMm_ImageBacking`.

## Fault Flow

The Strata page-fault handler receives the address-space reference, fault
address, and architecture error code:

1. Reject non-local or non-not-present faults.
2. Convert the faulting address to a virtual page.
3. Ask VMM to resolve the local page with `StVmm_ResolveLocalPage`.
4. Fetch `StVmm_PageInfo`.
5. Allocate one frame through PMM using the process allocation owner.
6. Map the frame through the local platform mapping path.
7. Apply zero-fill or image copy policy.

VMM resolution may also grow a downward-growing stack reservation by moving the
reservation base one page lower when the fault hits the grow-down guard page.

## Eager Versus Lazy

`MF_IMMEDIATE` requests eager materialization. Without it, local sparse
allocation and local image allocation may remain non-present until first access.

`MF_ZERO_FILL` is the content policy; it should not force the caller to know
whether the page is backed immediately or on demand. For user defaults,
`MF_USER_DEFAULT` includes `MF_ZERO_FILL`.

## Protection Changes

When userspace changes protections through the process remap path, MM must
update both existing PTEs and the reservation metadata. Pages that are still
non-present will later be materialized from the reservation's current
`StMm_MapFlags`; updating only present PTEs would leave future faulted pages
with stale protections.

## Failure Semantics

If resolving or materializing a page fails, the user fault path should terminate
or fail the affected process/thread path rather than turning every user fault
into a kernel panic. Kernel faults still represent stronger invariants and may
panic when the fault cannot be recovered.
