#ifndef __VELLUM_LOG_H__
#define __VELLUM_LOG_H__

#include <stdarg.h>

#include <vellum/compiler.h>

#define LL_NONE     -1
#define LL_FATAL    0
#define LL_ERROR    1
#define LL_WARN     2
#define LL_INFO     3
#define LL_DEBUG    4
#define LL_TRACE    5

void log_set_level(int level);

__format_printf(3, 4)
void log_printf(int level, const char *module_name, const char *fmt, ...);
__format_printf(3, 4)
void log_isr_printf(int level, const char *module_name, const char *fmt, ...);

void log_vprintf(int level, const char *module_name, const char *fmt, va_list args);
void log_isr_vprintf(int level, const char *module_name, const char *fmt, va_list args);

#define LOG(level, ...) log_printf(level, MODULE_NAME, __VA_ARGS__)
#define ILOG(level, ...) log_isr_printf(level, MODULE_NAME, __VA_ARGS__)
#define VLOG(level, fmt, args) log_vprintf(level, MODULE_NAME, fmt, args)
#define IVLOG(level, fmt, args) log_ivprintf(level, MODULE_NAME, fmt, args)

#define LOG_FATAL(...)  LOG(LL_FATAL, __VA_ARGS__);
#define LOG_ERROR(...)  LOG(LL_ERROR, __VA_ARGS__);
#define LOG_WARN(...)   LOG(LL_WARN, __VA_ARGS__);
#define LOG_INFO(...)   LOG(LL_INFO, __VA_ARGS__);
#define LOG_DEBUG(...)  LOG(LL_DEBUG, __VA_ARGS__);
#define LOG_TRACE(...)  LOG(LL_TRACE, __VA_ARGS__);

#define ILOG_FATAL(...) ILOG(LL_FATAL, __VA_ARGS__);
#define ILOG_ERROR(...) ILOG(LL_ERROR, __VA_ARGS__);
#define ILOG_WARN(...)  ILOG(LL_WARN, __VA_ARGS__);
#define ILOG_INFO(...)  ILOG(LL_INFO, __VA_ARGS__);
#define ILOG_DEBUG(...) ILOG(LL_DEBUG, __VA_ARGS__);
#define ILOG_TRACE(...) ILOG(LL_TRACE, __VA_ARGS__);

#endif // __VELLUM_LOG_H__
