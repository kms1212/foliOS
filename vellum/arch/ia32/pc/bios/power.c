#include <vellum/asm/power.h>

#include <stdint.h>

#include <uacpi/sleep.h>

#include <vellum/asm/interrupt.h>
#include <vellum/asm/intrinsics/idt.h>
#include <vellum/asm/io.h>

#include <vellum/panic.h>

#define MAKE_ACPI_STATUS(uacpi_status)                                                             \
    ((uacpi_status) ? (0x80010000 | (uacpi_status)) : STATUS_SUCCESS)

void _pc_reboot()
{
    struct idtr idtr = {0, 0};
    uint8_t status;

    interrupt_disable();

    do {
        status = io_in8(0x0064);
    } while (status & 0x02);
    io_out8(0x0064, 0xFE);

    _ia32_lidt(&idtr);
    __asm__ volatile("int $0xFF");

    __asm__ volatile("jmp $0xFFFF, $0x00000000");

    panic(STATUS_HARDWARE_FAILED, "how did you get here?");
}

void _pc_poweroff()
{
    interrupt_disable();

    io_out16(0xB004, 0x2000);
    io_out16(0x0604, 0x2000);
    io_out16(0x4004, 0x3400);

    panic(STATUS_HARDWARE_FAILED, "poweroff failed");
}
