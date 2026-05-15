#include <strata/mm/address_space.h>

#include <assert.h>

#include <strata/plat/memmap.h>
#include <strata/plat/mm.h>

#include <strata/compiler.h>
#include <strata/log.h>
#include <strata/mm/address_space_refs.h>
#include <strata/mm/pool.h>
#include <strata/mm/vmm.h>
#include <strata/process_refs.h>
#include <strata/status.h>
#include <strata/thread.h>
#include <strata/ref_control.h>

#define MODULE_NAME "mm"

struct StAddressSpace base_asp;

static StAddressSpace_InternalRef first_asp = &base_asp;
static StAddressSpace_InternalRef last_asp = &base_asp;

static void finalize_address_space(void *object)
{
    StAddressSpace_StrongRef asp = object;

    if (!asp || asp == &base_asp) return;

    StAddressSpaceP_Remove(asp);
    StPool_Free(asp);
}

static void unlink_address_space(StAddressSpace_InternalRef asp)
{
    StAddressSpace_InternalRef prev;

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

StStatus StAddressSpace_InitBase(void)
{
    base_asp.next = NULL;
    StRefControlBlock_Init(&base_asp.ref_control, 1, &base_asp, NULL);

    return StAddressSpaceP_InitBase();
}

StStatus StAddressSpace_Create(
    StAddressSpace_StrongRef *asp __out, StProcess_StrongRef process __in
)
{
    assert(asp);

    StStatus status;
    StAddressSpace_StrongRef new_asp = NULL;
    int p_asp_created = 0;

    status = StPool_AllocateClear(sizeof(*new_asp), (void **)&new_asp);
    if (!CHECK_SUCCESS(status)) goto has_error;
    StRefControlBlock_Init(&new_asp->ref_control, 1, new_asp, finalize_address_space);

    status = StAddressSpaceP_Create(new_asp);
    if (!CHECK_SUCCESS(status)) goto has_error;
    p_asp_created = 1;

    new_asp->process = (StProcess_InternalRef)process;

    status = StVmm_InitLocalDomain(new_asp, MEMMAP_USER_VPN_BASE, MEMMAP_USER_VPN_LIMIT);
    if (!CHECK_SUCCESS(status)) goto has_error;

    new_asp->next = NULL;

    StThread_LockPreemption();
    last_asp->next = (StAddressSpace_InternalRef)new_asp;
    last_asp = (StAddressSpace_InternalRef)new_asp;
    StThread_UnlockPreemption();

    *asp = new_asp;

    return STATUS_SUCCESS;

has_error:
    if (new_asp && p_asp_created) {
        StAddressSpaceP_Remove(new_asp);
    }

    if (new_asp) {
        StPool_Free(new_asp);
    }

    return status;
}

void StAddressSpace_Remove(StAddressSpace_StrongRef asp __in)
{
    if (!asp) return;
    if (asp == &base_asp) return;

    LOG_DEBUG(LM_CAT_UNCLASSIFIED, "removing address space\n");

    /*
     * Note: StProcess_Remove cleans up the owner allocation list (alloc_owner)
     * which frees the VMM pages. We don't need to iterate here because
     * StAddressSpace doesn't own the alloc_owner directly in the same way.
     */

    StThread_LockPreemption();
    StRefControlBlock_MarkDying(&asp->ref_control);
    unlink_address_space((StAddressSpace_InternalRef)asp);
    StThread_UnlockPreemption();

    StAddressSpace_Release(asp);
}

void StAddressSpace_Acquire(StAddressSpace_StrongRef asp __inout)
{
    assert(asp);

    StRefControlBlock_Acquire(&asp->ref_control);
}

void StAddressSpace_Release(StAddressSpace_StrongRef asp __inout)
{
    assert(asp);

    (void)StRefControlBlock_Release(&asp->ref_control);
}

StStatus StAddressSpace_Switch(StAddressSpace_StrongRef asp __in)
{
    return StAddressSpaceP_Switch(asp);
}
