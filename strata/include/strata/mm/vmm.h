#ifndef __STRATA_MM_VMM_H__
#define __STRATA_MM_VMM_H__

#include <stdint.h>

#include <strata/compiler.h>
#include <strata/status.h>
#include <strata/types.h>

#include <strata/mm/asp.h>
#include <strata/mm/types.h>

enum StVmm_Domain {
    VMM_DOMAIN_KERNEL_FAST = 0,
    VMM_DOMAIN_KERNEL_SLOW,
    VMM_DOMAIN_IO,
    VMM_DOMAIN_MODULE,
    VMM_DOMAIN_KRT_GLOBAL,
    VMM_DOMAIN_MAX,
};

StStatus StVmm_InitGlobalDomain(
    enum StVmm_Domain domain __in, St_VirtPage base_vpn __in, St_VirtPage limit_vpn __in
);
StStatus StVmm_InitLocalDomain(
    StMm_AddressSpace_StrongRef asp __in, St_VirtPage base_vpn __in, St_VirtPage limit_vpn __in
);
void StVmm_RemoveLocalDomain(StMm_AddressSpace_StrongRef asp __in);

StStatus StVmm_GetTotalGlobalPageCount(enum StVmm_Domain domain __in, St_PageCount *count __out);
StStatus StVmm_GetFreeGlobalPageCount(enum StVmm_Domain domain __in, St_PageCount *count __out);
StStatus StVmm_GetTotalLocalPageCount(
    StMm_AddressSpace_StrongRef asp __in, St_PageCount *count __out
);
StStatus StVmm_GetFreeLocalPageCount(StMm_AddressSpace_StrongRef asp __in, St_PageCount *count __out);

StStatus StVmm_AllocateGlobalPage(
    enum StVmm_Domain domain __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StMm_AllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in
);
StStatus StVmm_AllocateLocalPage(
    StMm_AddressSpace_StrongRef asp __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in
);

StStatus StVmm_AllocateGlobalPageTo(
    enum StVmm_Domain domain __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_AllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in
);
StStatus StVmm_AllocateLocalPageTo(
    StMm_AddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in
);

void StVmm_FreeGlobalPage(
    enum StVmm_Domain domain __in, St_VirtPage vpn __in, St_PageCount count __in
);
void StVmm_FreeLocalPage(
    StMm_AddressSpace_StrongRef asp __in, St_VirtPage vpn __in, St_PageCount count __in
);

StStatus StVmm_GetGlobalAllocationRange(
    enum StVmm_Domain domain __in,
    St_VirtPage vpn __in,
    St_VirtPage *begin_vpn __out_optional,
    St_VirtPage *end_vpn __out_optional
);

StStatus StVmm_GetLocalAllocationRange(
    StMm_AddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_VirtPage *begin_vpn __out_optional,
    St_VirtPage *end_vpn __out_optional
);

#endif  // __STRATA_MM_VMM_H__
