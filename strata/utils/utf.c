#include <strata/endian.h>
#include <strata/utf.h>

static int get_seq_len(St_Utf8Char c)
{
    if (((uint8_t)c & 0x80) == 0x00) return 1;
    if (((uint8_t)c & 0xE0) == 0xC0) return 2;
    if (((uint8_t)c & 0xF0) == 0xE0) return 3;
    if (((uint8_t)c & 0xF8) == 0xF0) return 4;
    return 0;
}

StStatus StUtf_CountUtf8Chars(
    const St_Utf8Char *src __in, size_t src_size __in, size_t *countout __out
)
{
    size_t i = 0;
    size_t count = 0;

    while (i < src_size) {
        uint8_t c = (uint8_t)src[i];
        int len = get_seq_len(c);
        int valid = 1;

        if (len == 0) {
            i++;
            count++;
            continue;
        }

        if (i + len > src_size) {
            i = src_size;
            count++;
            break;
        }

        if (len == 2) {
            if (((uint8_t)src[i + 1] & 0xC0) != 0x80) {
                valid = 0;
            } else if ((((c & 0x1F) << 6) | ((uint8_t)src[i + 1] & 0x3F)) < 0x80) {
                valid = 0;
            }
        } else if (len == 3) {
            if (((uint8_t)src[i + 1] & 0xC0) != 0x80 || ((uint8_t)src[i + 2] & 0xC0) != 0x80) {
                valid = 0;
            } else {
                uint32_t wc = ((c & 0x0F) << 12) | (((uint8_t)src[i + 1] & 0x3F) << 6) |
                    ((uint8_t)src[i + 2] & 0x3F);
                if (wc < 0x800 || (wc >= 0xD800 && wc <= 0xDFFF)) valid = 0;
            }
        } else if (len == 4) {
            if (((uint8_t)src[i + 1] & 0xC0) != 0x80 || ((uint8_t)src[i + 2] & 0xC0) != 0x80 ||
                ((uint8_t)src[i + 3] & 0xC0) != 0x80) {
                valid = 0;
            } else {
                uint32_t wc = ((c & 0x07) << 18) | (((uint8_t)src[i + 1] & 0x3F) << 12) |
                    (((uint8_t)src[i + 2] & 0x3F) << 6) | ((uint8_t)src[i + 3] & 0x3F);
                if (wc < 0x10000 || wc > UTF8_MAX_CODEPOINT) valid = 0;
            }
        }

        count++;
        i += (valid ? len : 1);
    }

    *countout = count;
    return STATUS_SUCCESS;
}

StStatus StUtf_Utf8ToUtf32(
    const St_Utf8Char *src __in,
    size_t src_size __in,
    St_Utf32Char *dest __in,
    size_t dest_size __in,
    size_t *countout __out
)
{
    size_t i = 0;
    size_t count = 0;

    while (i < src_size && count < dest_size) {
        uint8_t c = (uint8_t)src[i];
        uint32_t wc = UTF_REPLACEMENT_CODEPOINT;
        int len = get_seq_len(c);

        if (i + len > src_size) {
            i = src_size;
        } else if (len == 0) {
            i++;
        } else {
            int valid = 1;

            if (len == 1)
                wc = c;
            else if (len == 2) {
                if (((uint8_t)src[i + 1] & 0xC0) != 0x80) {
                    valid = 0;
                } else {
                    wc = ((c & 0x1F) << 6) | ((uint8_t)src[i + 1] & 0x3F);
                }
                if (wc < 0x80) valid = 0;
            } else if (len == 3) {
                if (((uint8_t)src[i + 1] & 0xC0) != 0x80 || ((uint8_t)src[i + 2] & 0xC0) != 0x80) {
                    valid = 0;
                } else {
                    wc = ((c & 0x0F) << 12) | (((uint8_t)src[i + 1] & 0x3F) << 6) |
                        ((uint8_t)src[i + 2] & 0x3F);
                }
                if (wc < 0x800 || (wc >= 0xD800 && wc <= 0xDFFF)) valid = 0;
            } else if (len == 4) {
                if (((uint8_t)src[i + 1] & 0xC0) != 0x80 || ((uint8_t)src[i + 2] & 0xC0) != 0x80 ||
                    ((uint8_t)src[i + 3] & 0xC0) != 0x80) {
                    valid = 0;
                } else {
                    wc = ((c & 0x07) << 18) | (((uint8_t)src[i + 1] & 0x3F) << 12) |
                        (((uint8_t)src[i + 2] & 0x3F) << 6) | ((uint8_t)src[i + 3] & 0x3F);
                }
                if (wc < 0x10000 || wc > UTF8_MAX_CODEPOINT) valid = 0;
            }

            if (!valid) {
                wc = UTF_REPLACEMENT_CODEPOINT;
                i++;
            } else {
                i += len;
            }
        }

        dest[count] = (St_Utf32Char)wc;
        count++;
    }

    if (count < dest_size) dest[count] = U'\0';

    if (countout) *countout = count;
    return STATUS_SUCCESS;
}

StStatus StUtf_Utf32ToUtf8(
    const St_Utf32Char *src __in,
    size_t src_size __in,
    St_Utf8Char *dest __in,
    size_t dest_size __in,
    size_t *countout __out
)
{
    size_t i = 0;
    size_t b = 0;

    while (i < src_size) {
        St_Utf32Char wc = src[i++];
        int needed = 0;

        if (wc < 0x80)
            needed = 1;
        else if (wc < 0x800)
            needed = 2;
        else if (wc < 0x10000)
            needed = 3;
        else if (wc <= UTF8_MAX_CODEPOINT)
            needed = 4;
        else
            needed = 3;

        /* 출력 버퍼 공간 확인 */
        if (b + needed > dest_size) {
            break;
        }

        if (needed == 1) {
            dest[b] = (St_Utf8Char)wc;
        } else if (needed == 2) {
            dest[b] = (St_Utf8Char)(0xC0 | (wc >> 6));
            dest[b + 1] = (St_Utf8Char)(0x80 | (wc & 0x3F));
        } else if (needed == 3) {
            if (wc > UTF8_MAX_CODEPOINT) {
                dest[b] = (St_Utf8Char)0xEF;
                dest[b + 1] = (St_Utf8Char)0xBF;
                dest[b + 2] = (St_Utf8Char)0xBD;
            } else {
                dest[b] = (St_Utf8Char)(0xE0 | (wc >> 12));
                dest[b + 1] = (St_Utf8Char)(0x80 | ((wc >> 6) & 0x3F));
                dest[b + 2] = (St_Utf8Char)(0x80 | (wc & 0x3F));
            }
        } else {  // needed == 4
            dest[b] = (St_Utf8Char)(0xF0 | (wc >> 18));
            dest[b + 1] = (St_Utf8Char)(0x80 | ((wc >> 12) & 0x3F));
            dest[b + 2] = (St_Utf8Char)(0x80 | ((wc >> 6) & 0x3F));
            dest[b + 3] = (St_Utf8Char)(0x80 | (wc & 0x3F));
        }

        b += needed;
    }

    if (b < dest_size) dest[b] = '\0';

    if (countout) *countout = b;
    return STATUS_SUCCESS;
}
