#include "internal.h"

#include <strata/mm.h>
#include <strata/mm/types.h>
#include <strata/mm/vmm.h>
#include <strata/panic.h>
#include <strata/status.h>
#include <strata/thread.h>

int liballoc_lock(void)
{
    StThread_LockPreemption();

    return 0;
}

int liballoc_unlock(void)
{
    StThread_UnlockPreemption();

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
        NULL,
        AF_DEFAULT,
        MF_KERNEL_DEFAULT
    );
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to allocate memory");
    }

    return PAGE_TO_VPTR(allocated_vpn);
}

int liballoc_free(void *vaddr, int page_count)
{
    St_VirtPage vpn = VPTR_TO_PAGE(vaddr);

    StMm_FreeGlobal(VMM_DOMAIN_KERNEL_SLOW, vpn, (St_PageCount)page_count);

    return 0;
}
