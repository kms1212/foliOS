
#include <uacpi/kernel_api.h>

#include <stdint.h>

#include <uacpi/platform/arch_helpers.h>
#include <uacpi/platform/types.h>
#include <uacpi/status.h>
#include <uacpi/types.h>

#include <strata/arch/intrinsics/misc.h>

#include <strata/plat/time.h>

#include <strata/log.h>
#include <strata/panic.h>
#include <strata/scheduler.h>
#include <strata/status.h>
#include <strata/thread.h>
#include <strata/thread_refs.h>

#define MODULE_NAME "acpi"

uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void)
{
    uint64_t uptime_ns;

    StTimeP_GetUptimeNanoseconds(&uptime_ns);

    return uptime_ns;
}

void uacpi_kernel_stall(uacpi_u8 usec)
{
    uint64_t start;
    uint64_t now_ns;

    StTimeP_GetUptimeNanoseconds(&start);
    StTimeP_GetUptimeNanoseconds(&now_ns);
    while (now_ns - start < (uint64_t)usec * 1000) {
        StA_Pause();
        StTimeP_GetUptimeNanoseconds(&now_ns);
    }
}

void uacpi_kernel_sleep(uacpi_u64 msec)
{
    StThread_Sleep((int)msec);
}

uacpi_thread_id uacpi_kernel_get_thread_id(void)
{
    StStatus status;
    StThread_InternalRef thread;

    status = StScheduler_GetCurrentThread(&thread);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to get current thread");
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
        St_Panic(
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
