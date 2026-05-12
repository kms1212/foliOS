#include <uacpi/kernel_api.h>

#include <stdatomic.h>

#include <uacpi/platform/types.h>
#include <uacpi/status.h>
#include <uacpi/types.h>

#include <strata/arch/interrupt.h>

#include <strata/interrupt.h>
#include <strata/log.h>
#include <strata/mm/pool.h>
#include <strata/plat/interrupt.h>
#include <strata/plat/interrupt_constants.h>
#include <strata/status.h>
#include <strata/thread.h>

#define MODULE_NAME "acpi"

struct acpi_irq_handle {
    struct StInt_Handler *interrupt_handler;
    uacpi_interrupt_handler handler;
    uacpi_handle ctx;
    uacpi_u32 irq;
    int vector;
    atomic_uint_fast32_t in_flight;
};

atomic_uint_fast32_t uacpi_kernel_interrupt_inflight_count = 0;

extern uacpi_status uacpi_kernel_ensure_work_thread(void);

static int irq_to_vector(uacpi_u32 irq)
{
    if (irq < 16) return LEGACY_IRQ_VECTOR_BASE + (int)irq;
    if (irq <= 0xFF) return (int)irq;

    return -1;
}

static void *interrupt_trampoline(
    int vector, struct StA_InterruptFrame *frame, struct StIntP_Context *ctx, void *data
)
{
    struct acpi_irq_handle *irq_handle = data;
    uacpi_interrupt_ret ret;

    (void)frame;
    (void)ctx;

    atomic_fetch_add(&uacpi_kernel_interrupt_inflight_count, 1);
    atomic_fetch_add(&irq_handle->in_flight, 1);

    ret = irq_handle->handler(irq_handle->ctx);
    if (ret == UACPI_INTERRUPT_NOT_HANDLED) {
        ILOG_TRACE(LM_CAT_ACPI, "ACPI interrupt #%02X not handled\n", vector);
    }

    atomic_fetch_sub(&irq_handle->in_flight, 1);
    atomic_fetch_sub(&uacpi_kernel_interrupt_inflight_count, 1);

    return NULL;
}

uacpi_status uacpi_kernel_install_interrupt_handler(
    uacpi_u32 irq, uacpi_interrupt_handler handler, uacpi_handle ctx, uacpi_handle *out_irq_handle
)
{
    StStatus status;
    struct acpi_irq_handle *irq_handle;
    int vector;

    if (!handler || !out_irq_handle) return UACPI_STATUS_INVALID_ARGUMENT;

    vector = irq_to_vector(irq);
    if (vector < 0) return UACPI_STATUS_INVALID_ARGUMENT;

    status = StPool_AllocateClear(sizeof(*irq_handle), (void **)&irq_handle);
    if (!CHECK_SUCCESS(status)) return UACPI_STATUS_OUT_OF_MEMORY;

    irq_handle->handler = handler;
    irq_handle->ctx = ctx;
    irq_handle->irq = irq;
    irq_handle->vector = vector;

    status = StInt_CreateHandler(
        vector,
        irq_handle,
        interrupt_trampoline,
        &irq_handle->interrupt_handler
    );
    if (!CHECK_SUCCESS(status)) {
        StPool_Free(irq_handle);
        return UACPI_STATUS_INTERNAL_ERROR;
    }

    status = uacpi_kernel_ensure_work_thread();
    if (status != UACPI_STATUS_OK) {
        StInt_RemoveHandler(irq_handle->interrupt_handler);
        StPool_Free(irq_handle);
        return status;
    }

    status = StIntP_Unmask(vector);
    if (!CHECK_SUCCESS(status)) {
        StInt_RemoveHandler(irq_handle->interrupt_handler);
        StPool_Free(irq_handle);
        return UACPI_STATUS_INTERNAL_ERROR;
    }

    LOG_DEBUG(LM_CAT_ACPI, "installed ACPI interrupt handler: irq=%u vector=%02X\n", irq, vector);

    *out_irq_handle = irq_handle;

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_uninstall_interrupt_handler(
    uacpi_interrupt_handler handler, uacpi_handle irq_handle
)
{
    struct acpi_irq_handle *handle = irq_handle;

    if (!handle) return UACPI_STATUS_INVALID_ARGUMENT;
    if (handler && handle->handler != handler) return UACPI_STATUS_INVALID_ARGUMENT;

    StInt_RemoveHandler(handle->interrupt_handler);

    while (atomic_load(&handle->in_flight)) {
        StThread_Yield();
    }

    LOG_DEBUG(
        LM_CAT_ACPI,
        "uninstalled ACPI interrupt handler: irq=%u vector=%02X\n",
        handle->irq,
        handle->vector
    );

    StPool_Free(handle);

    return UACPI_STATUS_OK;
}
