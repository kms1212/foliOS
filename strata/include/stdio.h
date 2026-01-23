#ifdef TESTING
#   include_next <stdio.h>

#else

#ifndef __STDIO_H__
#define __STDIO_H__

#include <stdarg.h>
#include <stddef.h>
#include <limits.h>

#include <strata/compiler.h>

__format_printf(3, 4)
int cprintf(int (*func)(void *, char), void *farg, const char *fmt, ...);
__format_printf(2, 3)
int sprintf(char *__restrict buf, const char *__restrict fmt, ...);
__format_printf(3, 4)
int snprintf(char *__restrict buf, size_t size, const char *__restrict fmt, ...);
int vcprintf(int (*func)(void *, char), void *farg, const char *fmt, va_list args);
int vsprintf(char *__restrict buf, const char *__restrict fmt, va_list args);
int vsnprintf(char *__restrict buf, size_t size, const char *__restrict fmt, va_list args);
int sscanf(const char *__restrict str, const char *__restrict fmt, ...);

#if __has_builtin(__builtin_sprintf)
#   define __HAVE_BUILTIN_SPRINTF

#endif

#if __has_builtin(__buitin_snprintf)
#   define __HAVE_BUILTIN_SNPRINTF

#endif

#if __has_builtin(__builtin_vsprintf)
#   define __HAVE_BUILTIN_VSPRINTF

#endif

#if __has_builtin(__buitin_vsnprintf)
#   define __HAVE_BUILTIN_VSNPRINTF

#endif

#endif // __STDIO_H__

#endif
