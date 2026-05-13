#include <strata/mm/asp.h>

#include <assert.h>

#include <strata/plat/memmap.h>
#include <strata/plat/mm.h>

#include <strata/compiler.h>
#include <strata/log.h>
#include <strata/mm/pool.h>
#include <strata/mm/vmm.h>
#include <strata/process.h>
#include <strata/status.h>
#include <strata/thread.h>

#define MODULE_NAME "mm"

struct StMm_AddressSpace base_asp;

static StMm_AddressSpace_InternalRef first_asp = &base_asp;
static StMm_AddressSpace_InternalRef last_asp = &base_asp;

static void finalize_address_space(void *object)
{
    StMm_AddressSpace_StrongRef asp = object;

    if (!asp || asp == &base_asp) return;

    StMmP_RemoveAddressSpace(asp);
    StPool_Free(asp);
}

static void unlink_address_space(StMm_AddressSpace_InternalRef asp)
{
    StMm_AddressSpace_InternalRef prev;

    if (!asp || asp == &base_asp) return;

    prev = &base_asp;
    while (prev->next && prev->next != asp) {
        prev = prev->next;
    }

    if (prev->next != asp) return;

    prev->next = asp->next;
    if (last_asp == asp) {
        last_asp = prev;
    }

    asp->next = NULL;
}

StStatus StMm_InitBaseAddressSpace(void)
{
    base_asp.next = NULL;
    StRefControlBlock_Init(&base_asp.ref_control, 1, &base_asp, NULL);

    return StMmP_InitBaseAddressSpace();
}

StStatus StMm_CreateAddressSpace(
    StMm_AddressSpace_StrongRef *asp __out, StProcess_StrongRef process __in
)
{
    assert(asp);

    StStatus status;
    StMm_AddressSpace_StrongRef new_asp = NULL;
    int p_asp_created = 0;
    int domain_initialized = 0;

    status = StPool_AllocateClear(sizeof(*new_asp), (void **)&new_asp);
    if (!CHECK_SUCCESS(status)) goto has_error;
    StRefControlBlock_Init(&new_asp->ref_control, 1, new_asp, finalize_address_space);

    status = StMmP_CreateAddressSpace(new_asp);
    if (!CHECK_SUCCESS(status)) goto has_error;
    p_asp_created = 1;

    new_asp->process = (StProcess_InternalRef)process;

    status = StVmm_InitLocalDomain(new_asp, MEMMAP_USER_VPN_BASE, MEMMAP_USER_VPN_LIMIT);
    if (!CHECK_SUCCESS(status)) goto has_error;
    domain_initialized = 1;

    new_asp->next = NULL;

    StThread_LockPreemption();
    last_asp->next = (StMm_AddressSpace_InternalRef)new_asp;
    last_asp = (StMm_AddressSpace_InternalRef)new_asp;
    StThread_UnlockPreemption();

    *asp = new_asp;

    return STATUS_SUCCESS;

has_error:
    if (new_asp && domain_initialized) {
        StVmm_RemoveLocalDomain(new_asp);
    }

    if (new_asp && p_asp_created) {
        StMmP_RemoveAddressSpace(new_asp);
    }

    if (new_asp) {
        StPool_Free(new_asp);
    }

    return status;
}

void StMm_RemoveAddressSpace(StMm_AddressSpace_StrongRef asp __in)
{
    if (!asp) return;
    if (asp == &base_asp) return;

    LOG_DEBUG(LM_CAT_UNCLASSIFIED, "removing address space\n");

    /*
     * Note: StProcess_Remove cleans up the owner allocation list (alloc_owner)
     * which frees the VMM pages. We don't need to iterate here because
     * StMm_AddressSpace doesn't own the alloc_owner directly in the same way.
     */

    StThread_LockPreemption();
    StRefControlBlock_MarkDying(&asp->ref_control);
    unlink_address_space((StMm_AddressSpace_InternalRef)asp);
    StThread_UnlockPreemption();

    StMm_ReleaseAddressSpace(asp);
}

void StMm_AcquireAddressSpace(StMm_AddressSpace_StrongRef asp __inout)
{
    assert(asp);

    StRefControlBlock_Acquire(&asp->ref_control);
}

void StMm_ReleaseAddressSpace(StMm_AddressSpace_StrongRef asp __inout)
{
    assert(asp);

    (void)StRefControlBlock_Release(&asp->ref_control);
}

StStatus StMm_SwitchAddressSpace(StMm_AddressSpace_StrongRef asp __in)
{
    return StMmP_SwitchAddressSpace(asp);
}
