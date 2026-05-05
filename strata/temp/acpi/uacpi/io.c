#include <uacpi/kernel_api.h>

#include <uacpi/platform/types.h>
#include <uacpi/status.h>

#include <strata/arch/intrinsics/io.h>

#include <strata/mm/pool.h>

#define MODULE_NAME "acpi"

struct iomap_data {
    uacpi_io_addr base;
    uacpi_size len;
};

uacpi_status uacpi_kernel_io_map(uacpi_io_addr base, uacpi_size len, uacpi_handle *out_handle)
{
    StStatus status;
    struct iomap_data *iomap_data;

    if (base + len > 0x10000) return UACPI_STATUS_INVALID_ARGUMENT;

    status = StPool_Allocate(sizeof(struct iomap_data), (void **)&iomap_data);
    if (!CHECK_SUCCESS(status)) return UACPI_STATUS_INTERNAL_ERROR;

    iomap_data->base = base;
    iomap_data->len = len;

    *out_handle = iomap_data;

    return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap(uacpi_handle handle)
{
    StPool_Free(handle);
}

uacpi_status uacpi_kernel_io_read8(uacpi_handle handle, uacpi_size offset, uacpi_u8 *out_value)
{
    struct iomap_data *iomap_data = handle;

    if (offset >= iomap_data->len) return UACPI_STATUS_INVALID_ARGUMENT;

    *out_value = StIoA_In8(iomap_data->base + offset);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read16(uacpi_handle handle, uacpi_size offset, uacpi_u16 *out_value)
{
    struct iomap_data *iomap_data = handle;

    if (offset >= iomap_data->len) return UACPI_STATUS_INVALID_ARGUMENT;

    *out_value = StIoA_In16(iomap_data->base + offset);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read32(uacpi_handle handle, uacpi_size offset, uacpi_u32 *out_value)
{
    struct iomap_data *iomap_data = handle;

    if (offset >= iomap_data->len) return UACPI_STATUS_INVALID_ARGUMENT;

    *out_value = StIoA_In32(iomap_data->base + offset);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write8(uacpi_handle handle, uacpi_size offset, uacpi_u8 in_value)
{
    struct iomap_data *iomap_data = handle;

    if (offset >= iomap_data->len) return UACPI_STATUS_INVALID_ARGUMENT;

    StIoA_Out8(iomap_data->base + offset, in_value);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write16(uacpi_handle handle, uacpi_size offset, uacpi_u16 in_value)
{
    struct iomap_data *iomap_data = handle;

    if (offset >= iomap_data->len) return UACPI_STATUS_INVALID_ARGUMENT;

    StIoA_Out16(iomap_data->base + offset, in_value);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write32(uacpi_handle handle, uacpi_size offset, uacpi_u32 in_value)
{
    struct iomap_data *iomap_data = handle;

    if (offset >= iomap_data->len) return UACPI_STATUS_INVALID_ARGUMENT;

    StIoA_Out32(iomap_data->base + offset, in_value);

    return UACPI_STATUS_OK;
}
