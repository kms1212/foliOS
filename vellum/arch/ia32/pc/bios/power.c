#include <vellum/plat/power.h>

#include <stdint.h>

#include <uacpi/sleep.h>

#include <vellum/arch/interrupt.h>
#include <vellum/arch/intrinsics/idt.h>
#include <vellum/arch/io.h>

#include <vellum/panic.h>

#define MAKE_ACPI_STATUS(uacpi_status)                                                             \
    ((uacpi_status) ? (0x80010000 | (uacpi_status)) : STATUS_SUCCESS)

void VlP_Reboot()
{
    struct VlA_Idtr idtr = {0, 0};
    uint8_t status;

    VlA_DisableInterrupt();

    do {
        status = VlA_In8(0x0064);
    } while (status & 0x02);
    VlA_Out8(0x0064, 0xFE);

    VlA_Lidt(&idtr);
    __asm__ volatile("int $0xFF");

    __asm__ volatile("jmp $0xFFFF, $0x00000000");

    VlP_Panic(STATUS_HARDWARE_FAILED, "how did you get here?");
}

void VlP_Poweroff()
{
    VlA_DisableInterrupt();

    VlA_Out16(0xB004, 0x2000);
    VlA_Out16(0x0604, 0x2000);
    VlA_Out16(0x4004, 0x3400);

    VlP_Panic(STATUS_HARDWARE_FAILED, "poweroff failed");
}
