#ifndef __VELLUM_LOG_H__
#define __VELLUM_LOG_H__

#include <stdarg.h>

#include <vellum/compiler.h>

#define LL_NONE  -1
#define LL_FATAL 0
#define LL_ERROR 1
#define LL_WARN  2
#define LL_INFO  3
#define LL_DEBUG 4
#define LL_TRACE 5

void VlLog_SetLevel(int level);

__format_printf(3, 4) void VlLog_Print(int level, const char *module_name, const char *fmt, ...);
__format_printf(3, 4) void VlLog_IntSafePrint(
    int level, const char *module_name, const char *fmt, ...
);

void VlLog_PrintValist(int level, const char *module_name, const char *fmt, va_list args);
void VlLog_IntSafePrintValist(int level, const char *module_name, const char *fmt, va_list args);

#define LOG(level, ...)         VlLog_Print(level, MODULE_NAME, __VA_ARGS__)
#define ILOG(level, ...)        VlLog_IntSafePrint(level, MODULE_NAME, __VA_ARGS__)
#define VLOG(level, fmt, args)  VlLog_PrintValist(level, MODULE_NAME, fmt, args)
#define IVLOG(level, fmt, args) VlLog_IntSafePrintValist(level, MODULE_NAME, fmt, args)

#define LOG_FATAL(...) LOG(LL_FATAL, __VA_ARGS__);
#define LOG_ERROR(...) LOG(LL_ERROR, __VA_ARGS__);
#define LOG_WARN(...)  LOG(LL_WARN, __VA_ARGS__);
#define LOG_INFO(...)  LOG(LL_INFO, __VA_ARGS__);
#define LOG_DEBUG(...) LOG(LL_DEBUG, __VA_ARGS__);
#define LOG_TRACE(...) LOG(LL_TRACE, __VA_ARGS__);

#define ILOG_FATAL(...) ILOG(LL_FATAL, __VA_ARGS__);
#define ILOG_ERROR(...) ILOG(LL_ERROR, __VA_ARGS__);
#define ILOG_WARN(...)  ILOG(LL_WARN, __VA_ARGS__);
#define ILOG_INFO(...)  ILOG(LL_INFO, __VA_ARGS__);
#define ILOG_DEBUG(...) ILOG(LL_DEBUG, __VA_ARGS__);
#define ILOG_TRACE(...) ILOG(LL_TRACE, __VA_ARGS__);

#endif  // __VELLUM_LOG_H__
