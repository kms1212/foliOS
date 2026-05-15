#ifndef __STRATA_MM_VMM_H__
#define __STRATA_MM_VMM_H__

#include <stdint.h>

#include <strata/compiler.h>
#include <strata/status.h>
#include <strata/types.h>

#include <strata/mm/address_space_refs.h>
#include <strata/mm/allocation_owner_refs.h>
#include <strata/mm/types.h>

enum StVmm_Domain {
    VMM_DOMAIN_KERNEL_FAST = 0,
    VMM_DOMAIN_KERNEL_SLOW,
    VMM_DOMAIN_IO,
    VMM_DOMAIN_MODULE,
    VMM_DOMAIN_KRT_GLOBAL,
    VMM_DOMAIN_MAX,
};

enum StVmm_PageMappingType {
    VMM_PAGE_MAPPING_PHYSICAL = 0,
    VMM_PAGE_MAPPING_DEMAND_ZERO,
    VMM_PAGE_MAPPING_IMAGE,
    VMM_PAGE_MAPPING_GUARD,
};

enum StVmm_PhysicalLayout {
    VMM_PHYSICAL_LAYOUT_CONTIGUOUS = 0,
    VMM_PHYSICAL_LAYOUT_SPARSE,
};

struct StVmm_PageMappingPolicy {
    enum StVmm_PageMappingType type;

    union {
        struct StMm_ImageBacking image;
    };
};

struct StVmm_PageInfo {
    StAllocationOwner_StrongRef owner;
    StMm_AllocFlags alloc_flags;
    StMm_MapFlags map_flags;
    enum StVmm_PhysicalLayout physical_layout;
    struct StVmm_PageMappingPolicy mapping_policy;
};

StStatus StVmm_InitGlobalDomain(
    enum StVmm_Domain domain __in, St_VirtPage base_vpn __in, St_VirtPage limit_vpn __in
);
StStatus StVmm_InitLocalDomain(
    StAddressSpace_StrongRef asp __in, St_VirtPage base_vpn __in, St_VirtPage limit_vpn __in
);
void StVmm_RemoveLocalDomain(StAddressSpace_StrongRef asp __in);

StStatus StVmm_GetTotalGlobalPageCount(enum StVmm_Domain domain __in, St_PageCount *count __out);
StStatus StVmm_GetFreeGlobalPageCount(enum StVmm_Domain domain __in, St_PageCount *count __out);
StStatus StVmm_GetTotalLocalPageCount(
    StAddressSpace_StrongRef asp __in, St_PageCount *count __out
);
StStatus StVmm_GetFreeLocalPageCount(StAddressSpace_StrongRef asp __in, St_PageCount *count __out);

StStatus StVmm_ReserveGlobalPage(
    enum StVmm_Domain domain __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
StStatus StVmm_ReserveLocalPage(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);

StStatus StVmm_ReserveGlobalPageTo(
    enum StVmm_Domain domain __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
StStatus StVmm_ReserveLocalPageTo(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
StStatus StVmm_ReserveLocalImagePageTo(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    const struct StMm_ImageBacking *image_backing __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);

void StVmm_ReleaseGlobalPage(
    enum StVmm_Domain domain __in, St_VirtPage vpn __in, St_PageCount count __in
);
void StVmm_ReleaseLocalPage(
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, St_PageCount count __in
);

StStatus StVmm_GetGlobalReservedRange(
    enum StVmm_Domain domain __in,
    St_VirtPage vpn __in,
    St_VirtPage *begin_vpn __out_optional,
    St_VirtPage *end_vpn __out_optional
);

StStatus StVmm_GetLocalReservedRange(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_VirtPage *begin_vpn __out_optional,
    St_VirtPage *end_vpn __out_optional
);

StStatus StVmm_GetGlobalPageInfo(
    enum StVmm_Domain domain __in,
    St_VirtPage vpn __in,
    struct StVmm_PageInfo *info __out
);
StStatus StVmm_GetLocalPageInfo(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    struct StVmm_PageInfo *info __out
);
StStatus StVmm_ResolveLocalPage(StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in);

#endif  // __STRATA_MM_VMM_H__
