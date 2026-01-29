#include <strata/log.h>

#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include <strata/plat/cpulocal.h>
#include <strata/plat/time.h>

#include <strata/process.h>

static int log_level = LL_DEFAULT;

static int (*log_print_func)(void *, char);
static void *log_print_state;

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

void StLog_Print(int level, const char *module_name, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    StLog_PrintValist(level, module_name, fmt, args);
    va_end(args);
}

void StLog_IntSafePrint(int level, const char *module_name, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    StLog_IntSafePrintValist(level, module_name, fmt, args);
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
    uint64_t uptime_us = StTimeP_GetUptimeMicroseconds();
    struct StCpuLocalP_Data *cpulocal = StCpuLocalP_GetData();
    struct StThread *thread = NULL;
    struct StProcess *process = NULL;

    if (cpulocal) {
        thread = cpulocal->scheduler.current;
        if (thread) {
            process = thread->process;
        }
    }

    cprintf(
        log_print_func,
        log_print_state,
        "%5" PRId64 ".%06" PRId64 " ",
        uptime_us / 1000000,
        uptime_us % 1000000
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

void StLog_PrintValist(int level, const char *module_name, const char *fmt, va_list args)
{
    if (log_level < level) return;

    print_log_header(level, module_name);
    vcprintf(log_print_func, log_print_state, fmt, args);
}

void StLog_IntSafePrintValist(int level, const char *module_name, const char *fmt, va_list args)
{
    if (log_level < level) return;

    print_log_header(level, module_name);
    vcprintf(log_print_func, log_print_state, fmt, args);
}
