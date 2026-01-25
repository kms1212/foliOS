#include <strata/log.h>

#include <stdio.h>

#include <strata/status.h>

void StLog_EarlyInit(int (*print_func)(void *, char), void *print_state) {}

void StLog_SetLevel(int level) {}

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
    "FATAL",
    "ERROR",
    "WARN",
    "INFO",
    "DEBUG",
    "TRACE",
};

void StLog_PrintValist(int level, const char *module_name, const char *fmt, va_list args)
{
    if (level >= LL_DEBUG) return;

    fprintf(stderr, "%s [%s] ", module_name, ll_str[level]);
    vfprintf(stderr, fmt, args);
}

void StLog_IntSafePrintValist(int level, const char *module_name, const char *fmt, va_list args)
{
    if (level >= LL_DEBUG) return;

    fprintf(stderr, "%s [%s] ", module_name, ll_str[level]);
    vfprintf(stderr, fmt, args);
}
