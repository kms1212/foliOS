#ifndef __STDIO_H__
#define __STDIO_H__

#ifdef TESTING
#    undef __STDIO_H__
#    include_next <stdio.h>

#else
#    include <limits.h>
#    include <stdarg.h>
#    include <stddef.h>

#    include <strata/compiler.h>

/*
 * Strata pointer-format extensions:
 *   %pU   - const struct StUuid *
 *   %puc  - const St_Utf32Char *
 *   %pus  - const nul-terminated St_Utf32Char *
 *   %*pus - const St_Utf32Char *, width is codepoint count
 *   %.*pus - const St_Utf32Char *, precision is codepoint count
 *   %*ph  - byte buffer as compact lowercase hex, width is byte count
 *   %*p<sep>h - byte buffer as lowercase hex separated by <sep>, width is byte count
 *   %pv   - virtual address as lowercase V:0x...
 *   %pV   - virtual address as uppercase V:0x...
 *   %pp   - const uintptr_t * physical address as lowercase P:0x...
 *   %pP   - const uintptr_t * physical address as uppercase P:0x...
 *   %b    - unsigned integer as binary
 */

__format_printf(3, 4) int cprintf(int (*func)(void *, char), void *farg, const char *fmt, ...);
__format_printf(2, 3) int sprintf(char *__restrict buf, const char *__restrict fmt, ...);
__format_printf(3, 4) int snprintf(
    char *__restrict buf, size_t size, const char *__restrict fmt, ...
);
int vcprintf(int (*func)(void *, char), void *farg, const char *fmt, va_list args);
int vsprintf(char *__restrict buf, const char *__restrict fmt, va_list args);
int vsnprintf(char *__restrict buf, size_t len, const char *__restrict fmt, va_list args);

#endif

#endif  // __STDIO_H__
