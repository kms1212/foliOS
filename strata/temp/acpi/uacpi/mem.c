#include <uacpi/kernel_api.h>

#include <stddef.h>

#include <uacpi/platform/types.h>

#include <strata/log.h>
#include <strata/mm.h>
#include <strata/mm/pool.h>
#include <strata/panic.h>
#include <strata/status.h>

#define MODULE_NAME "acpi"

void *uacpi_kernel_alloc(uacpi_size size)
{
    StStatus status;
    void *ptr;

    status = StPool_Allocate(size, &ptr);
    if (!CHECK_SUCCESS(status)) {
        LOG_ERROR(LM_CAT_UNCLASSIFIED, "StPool_Allocate(%zu) failed", size);
        return NULL;
    }

    return ptr;
}

void *uacpi_kernel_alloc_zeroed(uacpi_size size)
{
    StStatus status;
    void *ptr;

    status = StPool_AllocateClear(size, &ptr);
    if (!CHECK_SUCCESS(status)) {
        LOG_ERROR(LM_CAT_UNCLASSIFIED, "StPool_AllocateClear(%zu) failed", size);
        return NULL;
    }

    return ptr;
}

void uacpi_kernel_free(void *ptr)
{
    StStatus status;

    status = StPool_Free(ptr);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "StPool_Free(%p) failed", ptr);
    }
}
