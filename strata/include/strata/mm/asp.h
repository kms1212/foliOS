#ifndef __STRATA_MM_ASP_H__
#define __STRATA_MM_ASP_H__

#include <strata/plat/mm.h>

#include <strata/process.h>

struct StMm_AddressSpace {
    struct StMm_AddressSpace *next;

    struct StProcess *process;

    struct StMmP_AddressSpace platform_data;

    void *user_alloc_head;
    St_VirtPage user_base_vpn, user_limit_vpn;
    St_PageCount user_free_count;
};

StStatus StMm_InitBaseAddressSpace(void);

StStatus StMm_CreateAddressSpace(
    struct StMm_AddressSpace **asp __out, struct StProcess *process __in
);
void StMm_RemoveAddressSpace(struct StMm_AddressSpace *asp __in);
StStatus StMm_SwitchAddressSpace(struct StMm_AddressSpace *asp __in);

#endif  // __STRATA_MM_ASP_H__
