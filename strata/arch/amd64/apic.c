#include <strata/arch/apic.h>

#include <strata/arch/cpufeatures.h>
#include <strata/arch/intrinsics/msr.h>

#include <strata/plat/interrupt_constants.h>
#include <strata/plat/time.h>

#include <strata/mm.h>
#include <strata/mm/types.h>
#include <strata/status.h>

#define LAPIC_REG_SVR    0x00F0
#define LAPIC_SVR_ENABLE (1U << 8)

#define LAPIC_REG_EOI 0x00B0

#define LAPIC_REG_LVT_TIMER 0x0320
#define LAPIC_REG_TIMER_ICR 0x0380
#define LAPIC_REG_TIMER_CCR 0x0390
#define LAPIC_REG_TIMER_DCR 0x03E0

#define LAPIC_TIMER_MODE_ONESHOT  0x00000
#define LAPIC_TIMER_MODE_PERIODIC 0x20000
#define LAPIC_TIMER_MASK          0x10000

static uintptr_t lapic_mmio_base;
static int lapic_is_initialized;
static uint64_t lapic_ticks_per_sec;

StStatus StApicA_EnableGlobal(void)
{
    static int initialized = 0;

    StStatus status;
    uint64_t apic_base_msr_val;
    St_VirtPage lapic_mmio_vpn;

    if (initialized) return STATUS_ALREADY_PERFORMED;

    /* check capability */
    if (!g_p_cpu_features->has_apic) return STATUS_NOT_SUPPORTED;

    /* enable APIC */
    apic_base_msr_val = StA_ReadMsr(MSR_IA32_APIC_BASE);

    /* map LAPIC MMIO area */
    status = StMm_MapGlobal(
        VMM_DOMAIN_IO,
        &lapic_mmio_vpn,
        ADDR_TO_FRAME(apic_base_msr_val & IA32_APIC_BASE_ADDR_MASK),
        1,
        NULL,
        (struct StMm_CompoundFlags){AF_DEFAULT, MF_KERNEL_DEFAULT | MF_NO_CACHE}
    );
    if (!CHECK_SUCCESS(status)) return status;
    lapic_mmio_base = PAGE_TO_ADDR(lapic_mmio_vpn);

    initialized = 1;

    return STATUS_SUCCESS;
}

StStatus StApicA_EnableLocal(void)
{
    uint64_t apic_base_msr_val;

    /* check capability */
    if (!g_p_cpu_features->has_apic) return STATUS_NOT_SUPPORTED;

    /* enable APIC */
    apic_base_msr_val = StA_ReadMsr(MSR_IA32_APIC_BASE);
    if (!(apic_base_msr_val & IA32_APIC_BASE_ENABLE)) {
        StA_WriteMsr(MSR_IA32_APIC_BASE, apic_base_msr_val | IA32_APIC_BASE_ENABLE);
    }

    /* enable LAPIC */
    volatile uint32_t *svr = (volatile uint32_t *)(lapic_mmio_base + LAPIC_REG_SVR);
    *svr = *svr | LAPIC_SVR_ENABLE | SPURIOUS_IRQ_VECTOR;

    return STATUS_SUCCESS;
}

uint32_t StApicA_ReadLapicRegister(uintptr_t offset)
{
    return *(volatile uint32_t *)(lapic_mmio_base + offset);
}

void StApicA_WriteLapicRegister(uintptr_t offset, uint32_t value)
{
    *(volatile uint32_t *)(lapic_mmio_base + offset) = value;
}

void StApicA_SendEoi(void)
{
    StApicA_WriteLapicRegister(LAPIC_REG_EOI, 0);
}

StStatus StApicA_InitLapicTimer(void)
{
    uint32_t start_lapic, end_lapic;

    if (lapic_is_initialized) return STATUS_NOT_PERMITTED;

    StApicA_WriteLapicRegister(LAPIC_REG_TIMER_DCR, 0x03);

    StApicA_WriteLapicRegister(LAPIC_REG_TIMER_ICR, 0xFFFFFFFF);

    start_lapic = StApicA_ReadLapicRegister(LAPIC_REG_TIMER_CCR);

    StTimeP_EarlyBusyWaitMicroseconds(10000);

    end_lapic = StApicA_ReadLapicRegister(LAPIC_REG_TIMER_CCR);

    StApicA_WriteLapicRegister(LAPIC_REG_TIMER_ICR, 0);

    lapic_ticks_per_sec = (uint64_t)(start_lapic - end_lapic) * 100;
    lapic_is_initialized = 1;

    return STATUS_SUCCESS;
}

StStatus StApicA_SetLapicTimerPeriodic(uint64_t freq_hz __in)
{
    uint32_t ticks_per_interrupt;

    if (!lapic_is_initialized) return STATUS_NOT_PERMITTED;
    if (freq_hz == 0) return STATUS_INVALID_VALUE;

    ticks_per_interrupt = lapic_ticks_per_sec / freq_hz;
    if (ticks_per_interrupt == 0) return STATUS_INVALID_VALUE;

    StApicA_WriteLapicRegister(LAPIC_REG_TIMER_DCR, 0x03);
    StApicA_WriteLapicRegister(
        LAPIC_REG_LVT_TIMER,
        LAPIC_TIMER_MODE_PERIODIC | LAPIC_TIMER_IRQ_VECTOR
    );
    StApicA_WriteLapicRegister(LAPIC_REG_TIMER_ICR, ticks_per_interrupt);

    return STATUS_SUCCESS;
}

StStatus StApicA_SetLapicTimerOneshot(uint64_t us __in)
{
    if (!lapic_is_initialized) return STATUS_NOT_PERMITTED;

    uint64_t ticks = (lapic_ticks_per_sec * us) / 1000000;
    if (ticks == 0 || ticks > UINT32_MAX) return STATUS_INVALID_VALUE;

    StApicA_WriteLapicRegister(LAPIC_REG_TIMER_DCR, 0x03);
    StApicA_WriteLapicRegister(
        LAPIC_REG_LVT_TIMER,
        LAPIC_TIMER_MODE_ONESHOT | LAPIC_TIMER_IRQ_VECTOR
    );
    StApicA_WriteLapicRegister(LAPIC_REG_TIMER_ICR, ticks);

    return STATUS_SUCCESS;
}

void StApicA_StopLapicTimer(void)
{
    StApicA_WriteLapicRegister(LAPIC_REG_TIMER_ICR, 0);
    StApicA_WriteLapicRegister(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_MASK | LAPIC_TIMER_IRQ_VECTOR);
}
