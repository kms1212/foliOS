#ifndef __STRATA_LOG_H__
#define __STRATA_LOG_H__

#include <stdarg.h>
#include <stdint.h>

#include <strata/compiler.h>
#include <strata/status.h>

#define LL_NONE  -1
#define LL_FATAL 0
#define LL_ERROR 1
#define LL_WARN  2
#define LL_INFO  3
#define LL_DEBUG 4
#define LL_TRACE 5

#ifndef NDEBUG
#    define LL_DEFAULT LL_DEBUG
#    define LL_MAX     LL_TRACE

#else
#    define LL_DEFAULT LL_INFO
#    define LL_MAX     LL_DEBUG

#endif

#define LM_CATEGORY_MASK    ((uint32_t)0xFFFF0000)
#define LM_SUBCATEGORY_MASK ((uint32_t)0x0000FFFF)

#define LM_CAT_UNCLASSIFIED ((uint32_t)0x00000000)
#define LM_CAT_THREAD       ((uint32_t)0x00010000)
#define LM_CAT_ACPI         ((uint32_t)0xFFF00000)
#define LM_CAT_MAX          LM_CAT_THREAD

#define LM_SUBCAT_TASK_SWITCH ((uint32_t)0x00000001)

void StLog_EarlyInit(int (*print_func)(void *, char), void *print_state);

void StLog_SetLevel(int level);

StStatus StLog_GetMask(uint32_t category, uint16_t *mask);
StStatus StLog_SetMask(uint32_t mask);

__format_printf(4, 5) void StLog_Print(
    int level, uint32_t mask, const char *module_name, const char *fmt, ...
);
__format_printf(4, 5) void StLog_IntSafePrint(
    int level, uint32_t mask, const char *module_name, const char *fmt, ...
);

void StLog_PrintValist(
    int level, uint32_t mask, const char *module_name, const char *fmt, va_list args
);
void StLog_IntSafePrintValist(
    int level, uint32_t mask, const char *module_name, const char *fmt, va_list args
);

#define LOG(level, mask, ...)         StLog_Print(level, mask, MODULE_NAME, __VA_ARGS__)
#define ILOG(level, mask, ...)        StLog_IntSafePrint(level, mask, MODULE_NAME, __VA_ARGS__)
#define VLOG(level, mask, fmt, args)  StLog_PrintValist(level, mask, MODULE_NAME, fmt, args)
#define IVLOG(level, mask, fmt, args) StLog_IntSafePrintValist(level, mask, MODULE_NAME, fmt, args)

#define LOG_FATAL(mask, ...) LOG(LL_FATAL, mask, __VA_ARGS__);
#define LOG_ERROR(mask, ...) LOG(LL_ERROR, mask, __VA_ARGS__);
#define LOG_WARN(mask, ...)  LOG(LL_WARN, mask, __VA_ARGS__);
#define LOG_INFO(mask, ...)  LOG(LL_INFO, mask, __VA_ARGS__);
#define LOG_DEBUG(mask, ...) LOG(LL_DEBUG, mask, __VA_ARGS__);

#ifndef NDEBUG
#    define LOG_TRACE(mask, ...) LOG(LL_TRACE, mask, __VA_ARGS__);

#else
#    define LOG_TRACE(mask, ...) ((void)0)

#endif

#define ILOG_FATAL(mask, ...) ILOG(LL_FATAL, mask, __VA_ARGS__);
#define ILOG_ERROR(mask, ...) ILOG(LL_ERROR, mask, __VA_ARGS__);
#define ILOG_WARN(mask, ...)  ILOG(LL_WARN, mask, __VA_ARGS__);
#define ILOG_INFO(mask, ...)  ILOG(LL_INFO, mask, __VA_ARGS__);
#define ILOG_DEBUG(mask, ...) ILOG(LL_DEBUG, mask, __VA_ARGS__);

#ifndef NDEBUG
#    define ILOG_TRACE(mask, ...) ILOG(LL_TRACE, mask, __VA_ARGS__);

#else
#    define ILOG_TRACE(mask, ...) ((void)0)

#endif

#endif  // __STRATA_LOG_H__
