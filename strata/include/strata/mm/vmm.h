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
    VMM_DOMAIN_MAX,
};

StStatus StVmm_InitGlobalDomain(
    enum StVmm_Domain domain __in, St_VirtPage base_vpn __in, St_VirtPage limit_vpn __in
);
StStatus StVmm_InitLocalDomain(
    struct StMm_AddressSpace *asp __in, St_VirtPage base_vpn __in, St_VirtPage limit_vpn __in
);
StStatus StVmm_RemoveLocalDomain(struct StMm_AddressSpace *asp __in);

StStatus StVmm_GetTotalGlobalPageCount(enum StVmm_Domain domain __in, St_PageCount *count __out);
StStatus StVmm_GetFreeGlobalPageCount(enum StVmm_Domain domain, St_PageCount *count __out);
StStatus StVmm_GetTotalLocalPageCount(
    struct StMm_AddressSpace *asp __in, St_PageCount *count __out
);
StStatus StVmm_GetFreeLocalPageCount(struct StMm_AddressSpace *asp __in, St_PageCount *count __out);

StStatus StVmm_AllocateGlobalPage(
    enum StVmm_Domain domain __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StVmm_AllocFlags alloc_flags __in
);
StStatus StVmm_AllocateLocalPage(
    struct StMm_AddressSpace *asp __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StVmm_AllocFlags alloc_flags __in
);
void StVmm_FreeGlobalPage(St_VirtPage vpn __in, St_PageCount count __in);
void StVmm_FreeLocalPage(
    struct StMm_AddressSpace *asp __in, St_VirtPage vpn __in, St_PageCount count __in
);

#endif  // __STRATA_MM_VMM_H__
