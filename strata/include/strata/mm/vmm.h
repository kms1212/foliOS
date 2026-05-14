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

enum StVmm_BackingType {
    VMM_BACKING_PHYSICAL = 0,
    VMM_BACKING_DEMAND_ZERO,
    VMM_BACKING_IMAGE,
};

struct StVmm_PageInfo {
    enum StVmm_BackingType backing_type;
    StMm_AllocFlags alloc_flags;
    StMm_MapFlags map_flags;
    struct StMm_ImageBacking image_backing;
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

StStatus StVmm_AllocateGlobalPage(
    enum StVmm_Domain domain __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in
);
StStatus StVmm_AllocateLocalPage(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);

StStatus StVmm_AllocateGlobalPageTo(
    enum StVmm_Domain domain __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in
);
StStatus StVmm_AllocateLocalPageTo(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);

void StVmm_FreeGlobalPage(
    enum StVmm_Domain domain __in, St_VirtPage vpn __in, St_PageCount count __in
);
void StVmm_FreeLocalPage(
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, St_PageCount count __in
);

StStatus StVmm_GetGlobalAllocationRange(
    enum StVmm_Domain domain __in,
    St_VirtPage vpn __in,
    St_VirtPage *begin_vpn __out_optional,
    St_VirtPage *end_vpn __out_optional
);

StStatus StVmm_GetLocalAllocationRange(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_VirtPage *begin_vpn __out_optional,
    St_VirtPage *end_vpn __out_optional
);

StStatus StVmm_GetLocalPageInfo(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    struct StVmm_PageInfo *info __out
);
StStatus StVmm_SetLocalPageImageBacking(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    const struct StMm_ImageBacking *backing __in
);

#endif  // __STRATA_MM_VMM_H__
