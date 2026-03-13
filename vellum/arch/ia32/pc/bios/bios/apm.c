#include <vellum/plat/bios/apm.h>

#include <stdint.h>

#include <vellum/plat/bios/bioscall.h>

#define MAKE_STATUS(code) (code ? (0xA0001500 | (code)) : STATUS_SUCCESS)

status_t VlBiosP_CheckApmInstallation(
    uint16_t device_id, uint8_t *major_ver, uint8_t *minor_ver, uint16_t *flags
)
{
    struct bioscall_regs regs = {
        .a.w = 0x5300,
        .b.w = device_id,
    };

    if (VlBiosP_Call(0x15, &regs)) {
        return MAKE_STATUS(regs.a.b.h);
    }

    if (regs.b.w != 0x504D) {
        return STATUS_INVALID_SIGNATURE;
    }

    if (major_ver) *major_ver = regs.a.b.h;
    if (minor_ver) *minor_ver = regs.a.b.l;
    if (flags) *flags = regs.c.w;

    return STATUS_SUCCESS;
}
