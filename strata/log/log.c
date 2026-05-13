#include <strata/log.h>

#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include <strata/plat/cpulocal.h>
#include <strata/plat/time.h>

#include <strata/process.h>
#include <strata/status.h>
#include <strata/thread.h>

static int log_level = LL_DEFAULT;

static int (*log_print_func)(void *, char);
static void *log_print_state;

static uint16_t log_mask_list[LM_CAT_MAX >> 16];

void StLog_EarlyInit(int (*print_func)(void *, char), void *print_state)
{
    log_print_func = print_func;
    log_print_state = print_state;
}

void StLog_SetLevel(int level)
{
    if (level < -1) level = -1;
    if (level > LL_MAX) level = LL_MAX;

    log_level = level;
}

StStatus StLog_GetMask(uint32_t category, uint16_t *mask)
{
    if ((category & LM_CATEGORY_MASK) > LM_CAT_MAX) return STATUS_INVALID_VALUE;

    *mask = log_mask_list[category >> 16];

    return STATUS_SUCCESS;
}

StStatus StLog_SetMask(uint32_t mask)
{
    if ((mask & LM_CATEGORY_MASK) > LM_CAT_MAX) return STATUS_INVALID_VALUE;

    log_mask_list[mask >> 16] = mask & LM_SUBCATEGORY_MASK;

    return STATUS_SUCCESS;
}

void StLog_Print(int level, uint32_t mask, const char *module_name, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    StLog_PrintValist(level, mask, module_name, fmt, args);
    va_end(args);
}

void StLog_IntSafePrint(int level, uint32_t mask, const char *module_name, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    StLog_IntSafePrintValist(level, mask, module_name, fmt, args);
    va_end(args);
}

static const char *ll_str[] = {
    "FTL",
    "ERR",
    "WRN",
    "INF",
    "DBG",
    "TRC",
};

static void print_log_header(int level, const char *module_name)
{
    uint64_t uptime_ns;
    struct StCpuLocalP_Data *cpulocal = StCpuLocalP_GetData();
    StThread_InternalRef thread = NULL;
    struct StProcess *process = NULL;

    StTimeP_GetUptimeNanoseconds(&uptime_ns);

    if (cpulocal) {
        thread = cpulocal->scheduler.current_thread;
        if (thread) {
            process = thread->process;
        }
    }

    cprintf(
        log_print_func,
        log_print_state,
        "%5" PRId64 ".%06" PRId64 " ",
        uptime_ns / 1000 / 1000000,
        uptime_ns / 1000 % 1000000
    );

    if (!cpulocal) {
        cprintf(log_print_func, log_print_state, "--:----:---- ");
    } else if (!thread) {
        cprintf(log_print_func, log_print_state, "%02X:----:---- ", cpulocal->cpu_id);
    } else if (!process) {
        cprintf(log_print_func, log_print_state, "%02X:----:%04X ", cpulocal->cpu_id, thread->id);
    } else {
        cprintf(
            log_print_func,
            log_print_state,
            "%02X:%04X:%04X ",
            cpulocal->cpu_id,
            process->id,
            thread->id
        );
    }

    cprintf(log_print_func, log_print_state, "%3s %-9s # ", ll_str[level], module_name);
}

void StLog_PrintValist(
    int level, uint32_t mask, const char *module_name, const char *fmt, va_list args
)
{
    if (log_level < level) return;

    print_log_header(level, module_name);
    vcprintf(log_print_func, log_print_state, fmt, args);
}

void StLog_IntSafePrintValist(
    int level, uint32_t mask, const char *module_name, const char *fmt, va_list args
)
{
    if (log_level < level) return;

    print_log_header(level, module_name);
    vcprintf(log_print_func, log_print_state, fmt, args);
}
