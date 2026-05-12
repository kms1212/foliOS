#include "config.h"

#include <inttypes.h>
#include <stdint.h>

#if STRATA_ENABLE_ACPI
#    include <uacpi/acpi.h>
#    include <uacpi/status.h>
#    include <uacpi/tables.h>

#endif

#include <strata/arch/mmu_constants.h>

#include <strata/plat/hpet.h>

#include <strata/compiler.h>
#include <strata/log.h>
#include <strata/mm.h>
#include <strata/mm/types.h>
#include <strata/mm/vmm.h>
#include <strata/raw_spinlock.h>
#include <strata/status.h>

#define MODULE_NAME "hpet"

#define HPET_REG_GENERAL_CAP_ID    0x000
#define HPET_REG_GENERAL_CONFIG    0x010
#define HPET_REG_GENERAL_INTSTS    0x020
#define HPET_REG_MAIN_COUNTER      0x0F0
#define HPET_REG_TIMER0_CONFIG     0x100
#define HPET_REG_TIMER0_COMPARATOR 0x108

#define HPET_GENERAL_ENABLE             (1ULL << 0)
#define HPET_GENERAL_LEGACY_REPLACEMENT (1ULL << 1)

#define HPET_CAP_COUNTER_64BIT      (1ULL << 13)
#define HPET_CAP_LEGACY_REPLACEMENT (1ULL << 15)
#define HPET_CAP_PERIOD_FS_SHIFT    32

#define HPET_TIMER_INT_TYPE_LEVEL       (1ULL << 1)
#define HPET_TIMER_INT_ENABLE           (1ULL << 2)
#define HPET_TIMER_TYPE_PERIODIC        (1ULL << 3)
#define HPET_TIMER_PERIODIC_CAPABLE     (1ULL << 4)
#define HPET_TIMER_SET_COMPARATOR_VALUE (1ULL << 6)
#define HPET_TIMER_FORCE_32BIT          (1ULL << 8)

static uintptr_t hpet_mmio_base;
static uint64_t hpet_period_fs;
static uint64_t hpet_counter_hz;
static int hpet_counter_is_64bit;
static int hpet_initialized;
static uint64_t hpet_last_counter_value;
static struct StRawSpinlock hpet_lock;

static uint64_t read_hpet64(uintptr_t offset)
{
    return *(volatile uint64_t *)(hpet_mmio_base + offset);
}

static uint32_t read_hpet32(uintptr_t offset)
{
    return *(volatile uint32_t *)(hpet_mmio_base + offset);
}

static void write_hpet64(uintptr_t offset, uint64_t value)
{
    *(volatile uint64_t *)(hpet_mmio_base + offset) = value;
}

static uint64_t get_raw_main_counter(void)
{
    if (hpet_counter_is_64bit) {
        return read_hpet64(HPET_REG_MAIN_COUNTER);
    }

    return read_hpet32(HPET_REG_MAIN_COUNTER);
}

static uint64_t get_counter_delta_for_nanoseconds(uint64_t ns)
{
    __uint128_t ticks;

    if (!ns) return 0;

    ticks = (__uint128_t)ns * hpet_counter_hz;
    return (uint64_t)((ticks + 1000000000ULL - 1) / 1000000000ULL);
}

StStatus StHpetP_Init(void)
{
#if !STRATA_ENABLE_ACPI
    return STATUS_NOT_SUPPORTED;
#else
    StStatus status;
    uacpi_status uacpi_status;
    struct uacpi_table table;
    struct acpi_hpet *hpet;
    St_VirtPage mmio_vpn;
    uintptr_t mmio_phys_base, mmio_phys_offset;
    uint64_t capabilities;
    uint64_t config;

    if (hpet_initialized) return STATUS_ALREADY_PERFORMED;

    StRawSpinlock_Init(&hpet_lock);

    uacpi_status = uacpi_table_find_by_signature(ACPI_HPET_SIGNATURE, &table);
    if (uacpi_unlikely_error(uacpi_status)) {
        return STATUS_NOT_SUPPORTED;
    }

    hpet = table.ptr;
    if (hpet->address.address_space_id != 0) {
        return STATUS_NOT_SUPPORTED;
    }

    mmio_phys_base = hpet->address.address & ~((uintptr_t)PAGE_SIZE - 1);
    mmio_phys_offset = hpet->address.address - mmio_phys_base;

    status = StMm_MapGlobal(
        VMM_DOMAIN_IO,
        &mmio_vpn,
        ADDR_TO_FRAME(mmio_phys_base),
        1,
        NULL,
        (struct StMm_CompoundFlags){AF_DEFAULT, MF_KERNEL_DEFAULT | MF_NO_CACHE}
    );
    if (!CHECK_SUCCESS(status)) return status;

    hpet_mmio_base = PAGE_TO_ADDR(mmio_vpn) + mmio_phys_offset;

    capabilities = read_hpet64(HPET_REG_GENERAL_CAP_ID);
    if (!(capabilities & HPET_CAP_LEGACY_REPLACEMENT)) {
        return STATUS_NOT_SUPPORTED;
    }

    hpet_counter_is_64bit = !!(capabilities & HPET_CAP_COUNTER_64BIT);
    hpet_period_fs = capabilities >> HPET_CAP_PERIOD_FS_SHIFT;
    if (hpet_period_fs == 0) return STATUS_SYSTEM_CORRUPTED;

    hpet_counter_hz = 1000000000000000ULL / hpet_period_fs;
    if (hpet_counter_hz == 0) return STATUS_SYSTEM_CORRUPTED;

    config = read_hpet64(HPET_REG_GENERAL_CONFIG);
    config &= ~HPET_GENERAL_ENABLE;
    write_hpet64(HPET_REG_GENERAL_CONFIG, config);

    write_hpet64(HPET_REG_GENERAL_INTSTS, UINT64_MAX);
    write_hpet64(HPET_REG_MAIN_COUNTER, 0);

    config |= HPET_GENERAL_LEGACY_REPLACEMENT | HPET_GENERAL_ENABLE;
    write_hpet64(HPET_REG_GENERAL_CONFIG, config);

    hpet_last_counter_value = 0;
    hpet_initialized = 1;

    LOG_INFO(
        LM_CAT_UNCLASSIFIED,
        "HPET initialized: %" PRIu64 " Hz, %" PRIu64 " fs/tick, %s counter\n",
        hpet_counter_hz,
        hpet_period_fs,
        hpet_counter_is_64bit ? "64-bit" : "32-bit"
    );

    uacpi_table_unref(&table);

    return STATUS_SUCCESS;
#endif
}

int StHpetP_IsInitialized(void)
{
    return hpet_initialized;
}

uint64_t StHpetP_GetMainCounter(void)
{
    uint64_t raw_value;
    uint32_t irqstate;

    if (!hpet_initialized) return 0;

    raw_value = get_raw_main_counter();
    if (hpet_counter_is_64bit) {
        hpet_last_counter_value = raw_value;
        return raw_value;
    }

    StRawSpinlock_LockAndSaveIrq(&hpet_lock, &irqstate);

    if ((uint32_t)raw_value < (uint32_t)hpet_last_counter_value) {
        hpet_last_counter_value += 1ULL << 32;
    }
    hpet_last_counter_value =
        (hpet_last_counter_value & ~UINT32_C(0xFFFFFFFF)) | (uint32_t)raw_value;

    StRawSpinlock_UnlockAndRestoreIrq(&hpet_lock, irqstate);

    return hpet_last_counter_value;
}

uint64_t StHpetP_GetCounterFrequency(void)
{
    return hpet_counter_hz;
}

StStatus StHpetP_SetPeriodic(uint64_t freq_hz __in)
{
    uint64_t timer_config;
    uint64_t interval_ticks;
    uint64_t current_counter;

    if (!hpet_initialized) return STATUS_NOT_PERMITTED;
    if (freq_hz == 0) return STATUS_INVALID_VALUE;

    timer_config = read_hpet64(HPET_REG_TIMER0_CONFIG);
    if (!(timer_config & HPET_TIMER_PERIODIC_CAPABLE)) {
        return STATUS_NOT_SUPPORTED;
    }

    interval_ticks = hpet_counter_hz / freq_hz;
    if (interval_ticks == 0) return STATUS_INVALID_VALUE;

    timer_config &= ~(HPET_TIMER_INT_TYPE_LEVEL | HPET_TIMER_FORCE_32BIT);
    timer_config |=
        HPET_TIMER_INT_ENABLE | HPET_TIMER_TYPE_PERIODIC | HPET_TIMER_SET_COMPARATOR_VALUE;
    write_hpet64(HPET_REG_TIMER0_CONFIG, timer_config);

    current_counter = StHpetP_GetMainCounter();
    write_hpet64(HPET_REG_TIMER0_COMPARATOR, current_counter + interval_ticks);
    write_hpet64(HPET_REG_TIMER0_COMPARATOR, interval_ticks);

    timer_config &= ~HPET_TIMER_SET_COMPARATOR_VALUE;
    write_hpet64(HPET_REG_TIMER0_CONFIG, timer_config);

    return STATUS_SUCCESS;
}

StStatus StHpetP_SetOneshot(uint64_t ns __in)
{
    uint64_t timer_config;
    uint64_t delta_ticks;
    uint64_t current_counter;

    if (!hpet_initialized) return STATUS_NOT_PERMITTED;

    delta_ticks = get_counter_delta_for_nanoseconds(ns);
    if (delta_ticks == 0) return STATUS_INVALID_VALUE;

    timer_config = read_hpet64(HPET_REG_TIMER0_CONFIG);
    timer_config &=
        ~(HPET_TIMER_INT_TYPE_LEVEL | HPET_TIMER_TYPE_PERIODIC | HPET_TIMER_FORCE_32BIT);
    timer_config |= HPET_TIMER_INT_ENABLE;
    write_hpet64(HPET_REG_TIMER0_CONFIG, timer_config);

    current_counter = StHpetP_GetMainCounter();
    write_hpet64(HPET_REG_TIMER0_COMPARATOR, current_counter + delta_ticks);

    return STATUS_SUCCESS;
}

StStatus StHpetP_SetOneshotAndBusyWait(uint64_t ns __in)
{
    uint64_t start_counter;
    uint64_t target_delta;

    if (!hpet_initialized) return STATUS_NOT_PERMITTED;

    target_delta = get_counter_delta_for_nanoseconds(ns);
    if (target_delta == 0) return STATUS_INVALID_VALUE;

    start_counter = StHpetP_GetMainCounter();
    while ((StHpetP_GetMainCounter() - start_counter) < target_delta) {
    }

    return STATUS_SUCCESS;
}

void StHpetP_Stop(void)
{
    uint64_t timer_config;

    if (!hpet_initialized) return;

    timer_config = read_hpet64(HPET_REG_TIMER0_CONFIG);
    timer_config &= ~(HPET_TIMER_INT_ENABLE | HPET_TIMER_TYPE_PERIODIC);
    write_hpet64(HPET_REG_TIMER0_CONFIG, timer_config);
}
