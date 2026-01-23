#include <uacpi/kernel_api.h>

uacpi_thread_id uacpi_kernel_get_thread_id(void)
{
    return NULL;
}

uacpi_status uacpi_kernel_schedule_work(
    uacpi_work_type type,
    uacpi_work_handler handler,
    uacpi_handle ctx
)
{
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_wait_for_work_completion(void)
{
    return UACPI_STATUS_OK;
}
