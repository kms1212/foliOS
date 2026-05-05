#include <strata/scheduler.h>
#include <uacpi/kernel_api.h>

#include <uacpi/types.h>
#include <uacpi/platform/types.h>
#include <uacpi/status.h>

#include <strata/arch/intrinsics/misc.h>

#include <strata/log.h>
#include <strata/plat/panic.h>
#include <strata/plat/time.h>
#include <strata/status.h>

#include <strata/thread.h>

#define MODULE_NAME "acpi"

uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void)
{
    return StTimeP_GetUptimeNanoseconds();
}

void uacpi_kernel_stall(uacpi_u8 usec)
{
    uint64_t start = StTimeP_GetUptimeNanoseconds();

    while (StTimeP_GetUptimeNanoseconds() - start < (uint64_t)usec * 1000) {
        StA_Pause();
    }
}

void uacpi_kernel_sleep(uacpi_u64 msec)
{
    StThread_Sleep((int)msec);
}

uacpi_thread_id uacpi_kernel_get_thread_id(void)
{
    StStatus status;
    struct StThread *thread;

    status = StScheduler_GetCurrentThread(&thread);
    if (!CHECK_SUCCESS(status)) {
        StP_Panic(status, "failed to get current thread");
    }

    return thread;
}

uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request *req)
{
    if (!req) return UACPI_STATUS_INVALID_ARGUMENT;

    switch (req->type) {
    case UACPI_FIRMWARE_REQUEST_TYPE_BREAKPOINT:
        LOG_WARN(LM_CAT_ACPI, "ACPI firmware breakpoint request: ctx=%p\n", req->breakpoint.ctx);
        return UACPI_STATUS_OK;
    case UACPI_FIRMWARE_REQUEST_TYPE_FATAL:
        StP_Panic(
            STATUS_HARDWARE_FAILED,
            "ACPI firmware fatal request: type=%u code=%u arg=%" UACPI_PRIx64,
            req->fatal.type,
            req->fatal.code,
            UACPI_FMT64(req->fatal.arg)
        );
    default:
        LOG_WARN(LM_CAT_ACPI, "unknown ACPI firmware request type: %u\n", req->type);
        return UACPI_STATUS_INVALID_ARGUMENT;
    }
}
