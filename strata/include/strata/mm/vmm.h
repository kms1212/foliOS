#ifndef __STRATA_MM_VMM_H__
#define __STRATA_MM_VMM_H__

#include <stdint.h>

#include <strata/compiler.h>
#include <strata/status.h>
#include <strata/types.h>

#include <strata/mm/address_space_refs.h>
#include <strata/mm/allocation_owner_refs.h>
#include <strata/mm/types.h>

/** Global virtual-memory domains managed by VMM. */
enum StVmm_Domain {
    VMM_DOMAIN_KERNEL_FAST = 0,
    VMM_DOMAIN_KERNEL_SLOW,
    VMM_DOMAIN_IO,
    VMM_DOMAIN_MODULE,
    VMM_DOMAIN_KRT_GLOBAL,
    VMM_DOMAIN_MAX,
};

/** Per-page reservation policy recorded by VMM. */
enum StVmm_PageMappingType {
    /** Page is backed by physical frames selected by MM/PMM. */
    VMM_PAGE_MAPPING_PHYSICAL = 0,
    /** Page is materialized as zero-filled memory on first access. */
    VMM_PAGE_MAPPING_DEMAND_ZERO,
    /** Page is materialized from an image backing descriptor. */
    VMM_PAGE_MAPPING_IMAGE,
    /** Page is a guard and should not be made accessible as normal memory. */
    VMM_PAGE_MAPPING_GUARD,
};

/** Physical backing layout requested for a reservation. */
enum StVmm_PhysicalLayout {
    VMM_PHYSICAL_LAYOUT_CONTIGUOUS = 0,
    VMM_PHYSICAL_LAYOUT_SPARSE,
};

/** Mapping policy attached to a reserved VMM page range. */
struct StVmm_PageMappingPolicy {
    enum StVmm_PageMappingType type;

    union {
        /** Valid when type is VMM_PAGE_MAPPING_IMAGE. */
        struct StMm_ImageBacking image;
    };
};

/** Public snapshot of VMM metadata for one virtual page. */
struct StVmm_PageInfo {
    /** Allocation owner charged for this range, if any. */
    StAllocationOwner_StrongRef owner;
    StMm_AllocFlags alloc_flags;
    StMm_MapFlags map_flags;
    enum StVmm_PhysicalLayout physical_layout;
    struct StVmm_PageMappingPolicy mapping_policy;
};

/** Initialize a global VMM domain over the inclusive range [base_vpn, limit_vpn]. */
StStatus StVmm_InitGlobalDomain(
    enum StVmm_Domain domain __in, St_VirtPage base_vpn __in, St_VirtPage limit_vpn __in
);
/** Initialize the local user VMM domain over the inclusive range [base_vpn, limit_vpn]. */
StStatus StVmm_InitLocalDomain(
    StAddressSpace_StrongRef asp __in, St_VirtPage base_vpn __in, St_VirtPage limit_vpn __in
);
/** Remove all local VMM reservation metadata owned by an address space. */
void StVmm_RemoveLocalDomain(StAddressSpace_StrongRef asp __in);

/** Write total page capacity for a global domain. */
StStatus StVmm_GetTotalGlobalPageCount(enum StVmm_Domain domain __in, St_PageCount *count __out);
/** Write currently unreserved page capacity for a global domain. */
StStatus StVmm_GetFreeGlobalPageCount(enum StVmm_Domain domain __in, St_PageCount *count __out);
/** Write total page capacity for an address-space local domain. */
StStatus StVmm_GetTotalLocalPageCount(StAddressSpace_StrongRef asp __in, St_PageCount *count __out);
/** Write currently unreserved page capacity for an address-space local domain. */
StStatus StVmm_GetFreeLocalPageCount(StAddressSpace_StrongRef asp __in, St_PageCount *count __out);

/** Reserve a global range and let VMM choose the base page. */
StStatus StVmm_ReserveGlobalPage(
    enum StVmm_Domain domain __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
/** Reserve a local range and let VMM choose the base page. */
StStatus StVmm_ReserveLocalPage(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);

/** Reserve a global range at a chosen base page. */
StStatus StVmm_ReserveGlobalPageTo(
    enum StVmm_Domain domain __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
/** Reserve a local range at a chosen base page. */
StStatus StVmm_ReserveLocalPageTo(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
/** Reserve a local image-backed range at a chosen base page. */
StStatus StVmm_ReserveLocalImagePageTo(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    const struct StMm_ImageBacking *image_backing __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);

/** Release a global reservation range. */
void StVmm_ReleaseGlobalPage(
    enum StVmm_Domain domain __in, St_VirtPage vpn __in, St_PageCount count __in
);
/** Release a local reservation range. */
void StVmm_ReleaseLocalPage(
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, St_PageCount count __in
);

/** Find the reserved global range containing vpn. */
StStatus StVmm_GetGlobalReservedRange(
    enum StVmm_Domain domain __in,
    St_VirtPage vpn __in,
    St_VirtPage *begin_vpn __out_optional,
    St_VirtPage *end_vpn __out_optional
);

/** Find the reserved local range containing vpn. */
StStatus StVmm_GetLocalReservedRange(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_VirtPage *begin_vpn __out_optional,
    St_VirtPage *end_vpn __out_optional
);

/** Read VMM metadata for a global virtual page. */
StStatus StVmm_GetGlobalPageInfo(
    enum StVmm_Domain domain __in, St_VirtPage vpn __in, struct StVmm_PageInfo *info __out
);
/** Read VMM metadata for a local virtual page. */
StStatus StVmm_GetLocalPageInfo(
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, struct StVmm_PageInfo *info __out
);
/** Resolve local reservation policy before MM materializes a faulting page. */
StStatus StVmm_ResolveLocalPage(StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in);

#endif  // __STRATA_MM_VMM_H__
