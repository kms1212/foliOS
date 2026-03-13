#include <vellum/plat/apm.h>

#include <vellum/plat/bios/apm.h>
#include <vellum/plat/bios/bioscall.h>
#include <vellum/plat/farptr.h>
#include <vellum/plat/gdt.h>

#define MAKE_STATUS(code) (code ? (0xA0001500 | (code)) : STATUS_SUCCESS)

static status_t (*call_apm)(struct bioscall_regs *regs);
static uint16_t codeseg32, codeseg16, dataseg;
static void *entry;

extern int _pc_apm_call_pm32_interface(
    void *entry,
    uint16_t codeseg32,
    uint16_t codeseg16,
    uint16_t dataseg,
    struct bioscall_regs *regs
);
extern int _pc_apm_call_pm16_interface(
    void *entry, uint16_t codeseg16, uint16_t dataseg, struct bioscall_regs *regs
);

static status_t call_pm32_interface(struct bioscall_regs *regs)
{
    if (_pc_apm_call_pm32_interface(entry, codeseg32, codeseg16, dataseg, regs)) {
        return MAKE_STATUS(regs->a.b.h);
    }

    return STATUS_SUCCESS;
}

static status_t connect_pm32_interface(void)
{
    status_t status;
    uint16_t codeseg32_base, codeseg16_base, dataseg_base;
    uint16_t codeseg32_len, codeseg16_len, dataseg_len;

    status = _pc_bios_apm_connect_pm32_interface(
        &entry,
        &codeseg32_base,
        &codeseg16_base,
        &dataseg_base,
        &codeseg32_len,
        &codeseg16_len,
        &dataseg_len
    );
    if (status) return status;

    /* codeseg32 */
    _pc_gdt[5].limit_low = 0xFFFF;
    _pc_gdt[5].base_low = (codeseg32_base << 4) & 0xFFF0;
    _pc_gdt[5].base_mid = (codeseg32_base >> 12) & 0xF;
    _pc_gdt[5].base_high = 0x00;
    _pc_gdt[5].access_byte.raw = 0x9A;
    _pc_gdt[5].limit_flags.raw = 0xCF;
    codeseg32 = 0x28;

    /* codeseg16 */
    _pc_gdt[6].limit_low = 0x0FFF;
    _pc_gdt[6].base_low = (codeseg16_base << 4) & 0xFFF0;
    _pc_gdt[6].base_mid = (codeseg16_base >> 12) & 0xF;
    _pc_gdt[6].base_high = 0x00;
    _pc_gdt[6].access_byte.raw = 0x9A;
    _pc_gdt[6].limit_flags.raw = 0x80;
    codeseg16 = 0x30;

    /* dataseg */
    _pc_gdt[7].limit_low = 0x0FFF;
    _pc_gdt[7].base_low = (dataseg_base << 4) & 0xFFF0;
    _pc_gdt[7].base_mid = (dataseg_base >> 12) & 0xF;
    _pc_gdt[7].base_high = 0x00;
    _pc_gdt[7].access_byte.raw = 0x92;
    _pc_gdt[7].limit_flags.raw = 0x80;
    dataseg = 0x38;

    call_apm = call_pm32_interface;

    return STATUS_SUCCESS;
}

static status_t call_pm16_interface(struct bioscall_regs *regs)
{
    if (_pc_apm_call_pm16_interface(entry, codeseg16, dataseg, regs)) {
        return MAKE_STATUS(regs->a.b.h);
    }

    return STATUS_SUCCESS;
}

static status_t connect_pm16_interface(void)
{
    status_t status;
    uint16_t codeseg16_base, dataseg_base;
    uint16_t codeseg16_len, dataseg_len;

    status = _pc_bios_apm_connect_pm16_interface(
        &entry,
        &codeseg16_base,
        &dataseg_base,
        &codeseg16_len,
        &dataseg_len
    );
    if (status) return status;

    /* codeseg16 */
    _pc_gdt[5].limit_low = 0x0FFF;
    _pc_gdt[5].base_low = (codeseg16_base << 4) & 0xFFF0;
    _pc_gdt[5].base_mid = (codeseg16_base >> 12) & 0xF;
    _pc_gdt[5].base_high = 0x00;
    _pc_gdt[5].access_byte.raw = 0x9A;
    _pc_gdt[5].limit_flags.raw = 0x80;
    codeseg16 = 0x28;

    /* dataseg */
    _pc_gdt[6].limit_low = 0x0FFF;
    _pc_gdt[6].base_low = (dataseg_base << 4) & 0xFFF0;
    _pc_gdt[6].base_mid = (dataseg_base >> 12) & 0xF;
    _pc_gdt[6].base_high = 0x00;
    _pc_gdt[6].access_byte.raw = 0x92;
    _pc_gdt[6].limit_flags.raw = 0x80;
    dataseg = 0x30;

    call_apm = call_pm16_interface;

    return STATUS_SUCCESS;
}

static status_t call_rm_interface(struct bioscall_regs *regs)
{
    if (_pc_bios_call(0x15, regs)) {
        return MAKE_STATUS(regs->a.b.h);
    }

    return STATUS_SUCCESS;
}

static status_t connect_rm_interface(void)
{
    call_apm = call_rm_interface;

    return STATUS_SUCCESS;
}

status_t _pc_apm_init(void)
{
    status_t status;
    uint8_t major_ver, minor_ver;
    uint16_t flags;

    status = _pc_bios_apm_check_installation(0, &major_ver, &minor_ver, &flags);
    if (status) return status;

    if (flags & APM_PM32_SUPPORTED) {
        status = connect_pm32_interface();
    } else if (flags & APM_PM16_SUPPORTED) {
        status = connect_pm16_interface();
    } else {
        status = connect_rm_interface();
    }

    return STATUS_SUCCESS;
}
