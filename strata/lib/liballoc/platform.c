#include "internal.h"

#include <strata/mm.h>
#include <strata/panic.h>

#include <strata/arch/mmu.h>

int liballoc_lock(void)
{
    return 0;
}

int liballoc_unlock(void)
{
    return 0;
}

void *liballoc_alloc(int page_count)
{
    StStatus status;
    St_VirtPage allocated_vpn = (St_VirtPage)-1;

    status = StMm_AllocateGlobalSparse(
        VMM_DOMAIN_KERNEL_SLOW,
        &allocated_vpn,
        (St_PageCount)page_count,
        PMM_DEFAULT,
        VMM_DEFAULT,
        MAP_DEFAULT
    );
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to allocate memory");
    }

    return PAGE_TO_VPTR(allocated_vpn);
}

int liballoc_free(void *vaddr, int page_count)
{
    St_VirtPage vpn = VPTR_TO_PAGE(vaddr);

    StMm_FreeGlobal(vpn, (St_PageCount)page_count);

    return 0;
}
