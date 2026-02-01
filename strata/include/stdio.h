#ifdef TESTING
#    include_next <stdio.h>

#else

#    ifndef __STDIO_H__
#        define __STDIO_H__

#        include <limits.h>
#        include <stdarg.h>
#        include <stddef.h>

#        include <strata/compiler.h>

__format_printf(3, 4) int cprintf(int (*func)(void *, char), void *farg, const char *fmt, ...);
__format_printf(2, 3) int sprintf(char *__restrict buf, const char *__restrict fmt, ...);
__format_printf(3, 4) int snprintf(
    char *__restrict buf, size_t size, const char *__restrict fmt, ...
);
int vcprintf(int (*func)(void *, char), void *farg, const char *fmt, va_list args);
int vsprintf(char *__restrict buf, const char *__restrict fmt, va_list args);
int vsnprintf(char *__restrict buf, size_t size, const char *__restrict fmt, va_list args);
int sscanf(const char *__restrict str, const char *__restrict fmt, ...);

#    endif  // __STDIO_H__

#endif
