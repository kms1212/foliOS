#include <uacpi/kernel_api.h>

#include <vellum/asm/io.h>

uacpi_status uacpi_kernel_io_read8(uacpi_handle, uacpi_size offset, uacpi_u8 *out_value)
{
    *out_value = io_in8(offset);
    return 0;
}

uacpi_status uacpi_kernel_io_read16(uacpi_handle, uacpi_size offset, uacpi_u16 *out_value)
{
    *out_value = io_in16(offset);
    return 0;
}

uacpi_status uacpi_kernel_io_read32(uacpi_handle, uacpi_size offset, uacpi_u32 *out_value)
{
    *out_value = io_in32(offset);
    return 0;
}

uacpi_status uacpi_kernel_io_write8(uacpi_handle, uacpi_size offset, uacpi_u8 in_value)
{
    io_out8(offset, in_value);
    return 0;
}

uacpi_status uacpi_kernel_io_write16(uacpi_handle, uacpi_size offset, uacpi_u16 in_value)
{
    io_out16(offset, in_value);
    return 0;
}

uacpi_status uacpi_kernel_io_write32(uacpi_handle, uacpi_size offset, uacpi_u32 in_value)
{
    io_out32(offset, in_value);
    return 0;
}
