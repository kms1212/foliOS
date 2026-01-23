#include <strata/log.h>

#include <stdio.h>

#include <strata/arch/io.h>

#include <strata/status.h>

#ifdef NDEBUG
static int log_level = LL_NONE;

#else
static int log_level = LL_DEBUG;

#endif

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
    if (level > 4) level = 4;

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
    "FATAL", "ERROR", "WARN", "INFO", "DEBUG", "TRACE",
};

void StLog_PrintValist(int level, const char *module_name, const char *fmt, va_list args)
{
    if (log_level < level) return;

    cprintf(log_print_func, log_print_state, "%s [%s] ", module_name, ll_str[level]);
    vcprintf(log_print_func, log_print_state, fmt, args);
}

void StLog_IntSafePrintValist(int level, const char *module_name, const char *fmt, va_list args)
{
    if (log_level < level) return;
    
    cprintf(log_print_func, log_print_state, "%s [%s] ", module_name, ll_str[level]);
    vcprintf(log_print_func, log_print_state, fmt, args);
}
