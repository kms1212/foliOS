#ifndef __STRATA_MM_ADDRESS_SPACE_H__
#define __STRATA_MM_ADDRESS_SPACE_H__

#include <strata/plat/mm.h>

#include <strata/compiler.h>
#include <strata/mm/address_space_refs.h>
#include <strata/process_refs.h>
#include <strata/ref_control.h>
#include <strata/status.h>

struct StAddressSpace {
    struct StRefControlBlock ref_control;

    StAddressSpace_InternalRef next;

    StProcess_InternalRef process;

    struct StAddressSpaceP_PlatformData platform_data;

    void *user_reservation_head;
    St_VirtPage user_base_vpn, user_limit_vpn;
    St_PageCount user_free_count;
};

StStatus StAddressSpace_InitBase(void);

StStatus StAddressSpace_Create(
    StAddressSpace_StrongRef *asp __out, StProcess_StrongRef process __in
);
void StAddressSpace_Remove(StAddressSpace_StrongRef asp __in);
void StAddressSpace_Acquire(StAddressSpace_StrongRef asp __inout);
void StAddressSpace_Release(StAddressSpace_StrongRef asp __inout);
StStatus StAddressSpace_Switch(StAddressSpace_StrongRef asp __in);

#endif  // __STRATA_MM_ADDRESS_SPACE_H__
