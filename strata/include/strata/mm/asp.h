#ifndef __STRATA_MM_ASP_H__
#define __STRATA_MM_ASP_H__

#include <strata/plat/mm.h>

#include <strata/compiler.h>
#include <strata/ref_control.h>
#include <strata/status.h>

struct StProcess;

#ifndef __STRATA_MM_ADDRESS_SPACE_REFS_DEFINED__
#    define __STRATA_MM_ADDRESS_SPACE_REFS_DEFINED__
struct StMm_AddressSpace;
typedef struct StMm_AddressSpace *StMm_AddressSpace_StrongRef __ref_strong;
typedef struct StMm_AddressSpace *StMm_AddressSpace_WeakRef __ref_weak;
typedef struct StMm_AddressSpace *StMm_AddressSpace_BorrowedRef __ref_borrowed;
typedef struct StMm_AddressSpace *StMm_AddressSpace_InternalRef __ref_internal;
#endif

#ifndef __STRATA_PROCESS_REFS_DEFINED__
#    define __STRATA_PROCESS_REFS_DEFINED__
typedef struct StProcess *StProcess_StrongRef __ref_strong;
typedef struct StProcess *StProcess_WeakRef __ref_weak;
typedef struct StProcess *StProcess_BorrowedRef __ref_borrowed;
typedef struct StProcess *StProcess_InternalRef __ref_internal;
#endif

struct StMm_AddressSpace {
    struct StRefControlBlock ref_control;

    StMm_AddressSpace_InternalRef next;

    StProcess_InternalRef process;

    struct StMmP_AddressSpace platform_data;

    void *user_alloc_head;
    St_VirtPage user_base_vpn, user_limit_vpn;
    St_PageCount user_free_count;
};

StStatus StMm_InitBaseAddressSpace(void);

StStatus StMm_CreateAddressSpace(
    StMm_AddressSpace_StrongRef *asp __out, StProcess_StrongRef process __in
);
void StMm_RemoveAddressSpace(StMm_AddressSpace_StrongRef asp __in);
void StMm_AcquireAddressSpace(StMm_AddressSpace_StrongRef asp __inout);
void StMm_ReleaseAddressSpace(StMm_AddressSpace_StrongRef asp __inout);
StStatus StMm_SwitchAddressSpace(StMm_AddressSpace_StrongRef asp __in);

#endif  // __STRATA_MM_ASP_H__
