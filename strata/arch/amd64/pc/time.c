#include <strata/plat/time.h>

#include <inttypes.h>
#include <stdint.h>

#include <strata/arch/cpufeatures.h>
#include <strata/arch/intrinsics/msr.h>
#include <strata/arch/io.h>

#include <strata/log.h>

#define MODULE_NAME "time"

static int initialized = 0;
static int use_tsc;

static uint64_t uptime_start_counter;
static uint64_t counter_diff_per_sec;

static void calibrate_tsc_with_pit(void)
{
    StIoA_Out8(0x0043, 0x30);
    StIoA_Wait();
    StIoA_Out8(0x0040, 0xFF);
    StIoA_Wait();
    StIoA_Out8(0x0040, 0xFF);
    StIoA_Wait();

    uint64_t start_tsc = StA_ReadTsc();
    uint16_t elapsed_ticks;

    do {
        StIoA_Out8(0x0043, 0x00);

        uint16_t current_count = StIoA_In8(0x0040);
        current_count |= StIoA_In8(0x0040) << 8;

        elapsed_ticks = 65536 - current_count;
    } while (elapsed_ticks < 1193182 / 100);

    uint64_t end_tsc = StA_ReadTsc();

    counter_diff_per_sec = (end_tsc - start_tsc) * 100;
}

void StTimeP_StartUptime(void)
{
    if (g_p_cpu_features->has_tsc) {
        LOG_DEBUG(LM_CAT_UNCLASSIFIED, "using TSC for uptime counter\n");
        use_tsc = 1;
        uptime_start_counter = StA_ReadTsc();
        if (g_p_cpu_features->provides_tsc_ratio && g_p_cpu_features->provides_core_clock_freq) {
            LOG_DEBUG(LM_CAT_UNCLASSIFIED, "calibration skipped\n");
            counter_diff_per_sec = g_p_cpu_features->tsc_ratio_numer *
                g_p_cpu_features->core_clock_freq_hz / g_p_cpu_features->tsc_ratio_denom;
        } else {
            LOG_DEBUG(LM_CAT_UNCLASSIFIED, "calibrating TSC... (10 ms period)\n");
            calibrate_tsc_with_pit();
            LOG_DEBUG(
                LM_CAT_UNCLASSIFIED,
                "TSC calibrated: %" PRIu64 " delta per second\n",
                counter_diff_per_sec
            );
        }
    } else {
        LOG_DEBUG(LM_CAT_UNCLASSIFIED, "using PIT for uptime counter\n");
        use_tsc = 0;
        counter_diff_per_sec = 100;
        uptime_start_counter = StTimeP_GetGlobalTick();
    }
    initialized = 1;
}

uint64_t StTimeP_GetUptimeMicroseconds(void)
{
    if (!initialized) return 0;

    uint64_t current = use_tsc ? StA_ReadTsc() : StTimeP_GetGlobalTick();
    uint64_t ticks_diff = current - uptime_start_counter;

    return ticks_diff * 1000000 / counter_diff_per_sec;
}
