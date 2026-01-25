#ifndef __STRATA_UTF_H__
#define __STRATA_UTF_H__

#include <stdint.h>
#include <stddef.h>

#include <strata/status.h>
#include <strata/compiler.h>

typedef uint32_t St_Utf32Char __nocast;
typedef uint8_t St_Utf8Char __nocast;

#define UTF_REPLACEMENT_CODEPOINT   ((St_Utf32Char)0xFFFD)
#define UTF8_MAX_CODEPOINT          0x10FFFF

StStatus StUtf_CountUtf8Chars(
    const St_Utf8Char src[static 1] __in,
    size_t src_size __in,
    size_t *count __out
);

StStatus StUtf_Utf8ToUtf32(
    const St_Utf8Char src[static 1] __in,
    size_t src_size __in,
    St_Utf32Char dest[static 1] __in,
    size_t dest_size __in,
    size_t *count __out
);

StStatus StUtf_Utf32ToUtf8(
    const St_Utf32Char src[static 1] __in,
    size_t src_size __in,
    St_Utf8Char dest[static 1] __in,
    size_t dest_size __in,
    size_t *count __out
);

#endif // __STRATA_UTF_H__
