#include <uacpi/kernel_api.h>

#include <stddef.h>

#include <uacpi/platform/types.h>
#include <uacpi/status.h>

#include <strata/mm/pool.h>
#include <strata/mutex.h>
#include <strata/status.h>

#define MODULE_NAME "acpi"

uacpi_handle uacpi_kernel_create_mutex(void)
{
    StStatus status;
    struct StMutex *mutex;

    status = StPool_Allocate(sizeof(*mutex), (void **)&mutex);
    if (!CHECK_SUCCESS(status)) return NULL;

    StMutex_Init(mutex);

    return mutex;
}

void uacpi_kernel_free_mutex(uacpi_handle mutex)
{
    StPool_Free(mutex);
}

uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle mutex, uacpi_u16 timeout_ms)
{
    StStatus status;
    int lock_success = 0;

    if (!timeout_ms) {
        status = StMutex_TryLock(mutex, &lock_success);
    } else if (timeout_ms == UINT16_MAX) {
        status = StMutex_Lock(mutex);
        lock_success = CHECK_SUCCESS(status);
    } else {
        status = StMutex_LockWithTimeout(mutex, (int)timeout_ms);
        lock_success = (status != STATUS_TIMER_EXPIRED) && CHECK_SUCCESS(status);
    }
    if (status != STATUS_TIMER_EXPIRED && !CHECK_SUCCESS(status)) {
        return UACPI_STATUS_INTERNAL_ERROR;
    }

    return lock_success ? UACPI_STATUS_OK : UACPI_STATUS_TIMEOUT;
}

void uacpi_kernel_release_mutex(uacpi_handle mutex)
{
    StMutex_Unlock(mutex);
}
