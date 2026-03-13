#ifndef __STRATA_UTF_H__
#define __STRATA_UTF_H__

#include <stddef.h>
#include <stdint.h>

#include <strata/compiler.h>
#include <strata/status.h>

typedef uint32_t St_Utf32Char __nocast;
typedef uint8_t St_Utf8Char __nocast;

#define UTF_REPLACEMENT_CODEPOINT ((St_Utf32Char)0xFFFD)
#define UTF8_MAX_CODEPOINT        0x10FFFF

StStatus StUtf_CountUtf8Chars(
    const St_Utf8Char *str __in, size_t str_size __in, size_t *count __out
);

StStatus StUtf_Utf8ToUtf32(
    const St_Utf8Char *src __in,
    size_t src_size __in,
    St_Utf32Char *dest __buf,
    size_t dest_size __in,
    size_t *count __out
);

StStatus StUtf_Utf32ToUtf8(
    const St_Utf32Char *src __in,
    size_t src_size __in,
    St_Utf8Char *dest __buf,
    size_t dest_size __in,
    size_t *count __out
);

#endif  // __STRATA_UTF_H__
