#include <strata/mm/asp.h>

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

static struct StMm_AddressSpace *first_asp = &base_asp;
static struct StMm_AddressSpace *last_asp = &base_asp;

static void unlink_address_space(struct StMm_AddressSpace *asp)
{
    struct StMm_AddressSpace *prev;

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

    return StMmP_InitBaseAddressSpace();
}

StStatus StMm_CreateAddressSpace(
    struct StMm_AddressSpace **asp __out, struct StProcess *process __in
)
{
    StStatus status;
    struct StMm_AddressSpace *new_asp = NULL;
    int p_asp_created = 0;
    int domain_initialized = 0;

    status = StPool_AllocateClear(sizeof(*new_asp), (void **)&new_asp);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = StMmP_CreateAddressSpace(new_asp);
    if (!CHECK_SUCCESS(status)) goto has_error;
    p_asp_created = 1;

    new_asp->process = process;

    status = StVmm_InitLocalDomain(new_asp, MEMMAP_USER_VPN_BASE, MEMMAP_USER_VPN_LIMIT);
    if (!CHECK_SUCCESS(status)) goto has_error;
    domain_initialized = 1;

    new_asp->next = NULL;

    StThread_LockPreemption();
    last_asp->next = new_asp;
    last_asp = new_asp;
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

void StMm_RemoveAddressSpace(struct StMm_AddressSpace *asp __in)
{
    if (!asp) return;

    LOG_DEBUG(LM_CAT_UNCLASSIFIED, "removing address space\n");

    /*
     * Note: StProcess_Remove cleans up the owner allocation list (alloc_owner)
     * which frees the VMM pages. We don't need to iterate here because
     * StMm_AddressSpace doesn't own the alloc_owner directly in the same way.
     */

    StThread_LockPreemption();
    unlink_address_space(asp);
    StThread_UnlockPreemption();

    StMmP_RemoveAddressSpace(asp);
    StPool_Free(asp);
}

StStatus StMm_SwitchAddressSpace(struct StMm_AddressSpace *asp __in)
{
    return StMmP_SwitchAddressSpace(asp);
}
