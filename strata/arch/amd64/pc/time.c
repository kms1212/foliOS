#include "config.h"

#include <strata/plat/time.h>

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

#include <strata/arch/interrupt.h>

#include <strata/plat/hpet.h>
#include <strata/plat/interrupt.h>
#include <strata/plat/interrupt_constants.h>
#include <strata/plat/pit.h>

#include <strata/compiler.h>
#include <strata/interrupt.h>
#include <strata/log.h>
#include <strata/status.h>

#define MODULE_NAME "time"

static int initialized = 0;
static int use_hpet;

static uint64_t uptime_start_counter;
static uint64_t uptime_counter_freq;

static atomic_uint_fast64_t global_tick = 0;

static void *tick_isr(
    int num, struct StA_InterruptFrame *frame, struct StIntP_Context *ctx, void *data
)
{
    atomic_fetch_add(&global_tick, 1);

    return NULL;
}

void StTimeP_EarlyBusyWaitNanoseconds(uint64_t ns)
{
    if (!ns) return;

    if (StHpetP_IsInitialized()) {
        if (CHECK_SUCCESS(StHpetP_SetOneshotAndBusyWait(ns))) return;
    }

    StPitP_SetOneshotAndBusyWait(ns);
}

void StTimeP_InitTimer(int _use_hpet __in)
{
    use_hpet = _use_hpet;

    if (use_hpet) {
        uptime_start_counter = StHpetP_GetMainCounter();
        uptime_counter_freq = StHpetP_GetCounterFrequency();

        if (CHECK_SUCCESS(StHpetP_SetPeriodic(STRATA_TICK_RATE_HZ))) {
            LOG_INFO(
                LM_CAT_UNCLASSIFIED,
                "Clock source initialized: HPET main counter, %dHz tick\n",
                STRATA_TICK_RATE_HZ
            );
        } else {
            use_hpet = 0;
        }
    }

    if (!use_hpet) {
        LOG_DEBUG(LM_CAT_UNCLASSIFIED, "using PIT tick for uptime counter\n");
        uptime_counter_freq = STRATA_TICK_RATE_HZ;
        uptime_start_counter = StTimeP_GetGlobalTick();

        StPitP_SetPeriodic(STRATA_TICK_RATE_HZ);

        LOG_INFO(
            LM_CAT_UNCLASSIFIED,
            "Clock source initialized: PIT channel 0, %dHz\n",
            STRATA_TICK_RATE_HZ
        );
    } else {
        StPitP_Stop();
    }
    initialized = 1;

    StInt_CreateHandler(use_hpet ? HPET_IRQ_VECTOR : LEGACY_IRQ_VECTOR_BASE, NULL, tick_isr, NULL);
}

StStatus StTimeP_StartTimer(void)
{
    if (!initialized) return STATUS_NOT_PERMITTED;

    StIntP_Unmask(use_hpet ? HPET_IRQ_VECTOR : LEGACY_IRQ_VECTOR_BASE);

    return STATUS_SUCCESS;
}

uint64_t StTimeP_GetUptimeNanoseconds(void)
{
    if (!initialized) return 0;

    uint64_t current = use_hpet ? StHpetP_GetMainCounter() : StTimeP_GetGlobalTick();
    uint64_t ticks_diff = current - uptime_start_counter;

    return ((__uint128_t)ticks_diff * 1000000000ULL) / uptime_counter_freq;
}

uint64_t StTimeP_GetGlobalTick(void)
{
    return global_tick;
}

uint32_t StTimeP_GetGlobalTickFrequency(void)
{
    return STRATA_TICK_RATE_HZ;
}
