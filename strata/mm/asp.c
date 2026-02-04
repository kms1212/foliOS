#include <strata/mm/asp.h>

#include <stdlib.h>

#include <strata/plat/memmap.h>

#include <strata/mm.h>

struct StMm_AddressSpace base_asp;

static struct StMm_AddressSpace *first_asp = &base_asp;
static struct StMm_AddressSpace *last_asp = &base_asp;

StStatus StMm_InitBaseAddressSpace(void)
{
    base_asp.next = NULL;

    return StMmP_InitBaseAddressSpace();
}

StStatus StMm_CreateAddressSpace(struct StMm_AddressSpace **asp __out)
{
    StStatus status;
    struct StMm_AddressSpace *new_asp;
    int p_asp_created = 0;
    int domain_initialized = 0;

    status = StPool_AllocateClear(sizeof(*new_asp), (void **)&new_asp);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = StMmP_CreateAddressSpace(new_asp);
    if (!CHECK_SUCCESS(status)) goto has_error;
    p_asp_created = 1;

    status = StVmm_InitLocalDomain(new_asp, MEMMAP_USER_VPN_BASE, MEMMAP_USER_VPN_LIMIT);
    if (!CHECK_SUCCESS(status)) goto has_error;
    domain_initialized = 1;

    new_asp->next = NULL;

    last_asp->next = new_asp;
    last_asp = new_asp;

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
    StMmP_RemoveAddressSpace(asp);
    StPool_Free(asp);
}

StStatus StMm_SwitchAddressSpace(struct StMm_AddressSpace *asp __in)
{
    return StMmP_SwitchAddressSpace(asp);
}
