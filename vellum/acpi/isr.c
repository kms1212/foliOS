#include <uacpi/kernel_api.h>

#include <vellum/asm/isr.h>

uacpi_status uacpi_kernel_install_interrupt_handler(
    uacpi_u32 irq, uacpi_interrupt_handler handler, uacpi_handle ctx, uacpi_handle *out_irq_handle
)
{
    status_t status;

    status = _pc_isr_add_interrupt_handler(
        (int)irq,
        ctx,
        (interrupt_handler_t)handler,
        (struct isr_handler **)out_irq_handle
    );
    if (!CHECK_SUCCESS(status)) return UACPI_STATUS_INTERNAL_ERROR;

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_uninstall_interrupt_handler(
    uacpi_interrupt_handler, uacpi_handle irq_handle
)
{
    _pc_isr_remove_handler(irq_handle);

    return UACPI_STATUS_OK;
}
