#include <uacpi/kernel_api.h>

#include <vellum/asm/intrinsics/rdtsc.h>

uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void)
{
    return _ia32_rdtsc();
}

void uacpi_kernel_stall(uacpi_u8 usec)
{
    for (volatile int i = 0; i < (usec << 4); i++) {
    }
}

void uacpi_kernel_sleep(uacpi_u64 msec)
{
    for (volatile uint64_t i = 0; i < (msec << 8); i++) {
    }
}
