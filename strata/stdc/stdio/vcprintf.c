#include <stdio.h>

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>

#include <strata/macros.h>
#include <strata/utf.h>
#include <strata/uuid.h>

#define SF_LEFT   0x01
#define SF_PLUS   0x02
#define SF_SPACE  0x04
#define SF_ZERO   0x08
#define SF_LOWER  0x10
#define SF_PREFIX 0x20
#define SF_PTR    0x40
#define SF_PREC   0x80

#define WIDTH_AUTO 0
#define PREC_ARG   (-1)
#define WIDTH_ARG  (-1)

enum fmt_spec_type {
    INVALID = 0,
    PERCENT,
    CHAR,
    STR,
    PTR,
    ULONG_LONG,
    LONG_LONG,
    ULONG,
    LONG,
    UBYTE,
    BYTE,
    USHORT,
    SHORT,
    UINT,
    INT,
    UINTMAX_T,
    INTMAX_T,
    SIZE_T,
    PTRDIFF_T,
};

enum ptr_fmt_type {
    PTR_FMT_DEFAULT = 0,
    PTR_FMT_UUID,
    PTR_FMT_UTF32_CHAR,
    PTR_FMT_UTF32_STRING,
    PTR_FMT_HEX_BYTES,
    PTR_FMT_VADDR_LOWER,
    PTR_FMT_VADDR_UPPER,
    PTR_FMT_PADDR_LOWER,
    PTR_FMT_PADDR_UPPER,
};

struct fmt_spec {
    int width;
    int precision;
    uint8_t flags;
    uint8_t base;
    uint8_t type;
    uint8_t ptr_format;
    char ptr_separator;
    const char *next;
};

static struct fmt_spec decode_spec(const char *fmt)
{
    struct fmt_spec spec = {0};

    // flags
    while (*fmt) {
        switch (*fmt) {
        case '-':
            spec.flags |= SF_LEFT;
            break;
        case '+':
            spec.flags |= SF_PLUS;
            break;
        case ' ':
            spec.flags |= SF_SPACE;
            break;
        case '#':
            spec.flags |= SF_PREFIX;
            break;
        case '0':
            spec.flags |= SF_ZERO;
            break;
        default:
            goto end_flags;
        }
        fmt++;
    }
end_flags:

    // width
    if (*fmt == '*') {  // from va_args
        spec.width = WIDTH_ARG;
        fmt++;
    } else {  // width specified
        while (*fmt) {
            if (isdigit(*fmt)) {
                spec.width *= 10;
                spec.width += *fmt - '0';
            } else {
                goto end_width;
            }
            fmt++;
        }
    }
end_width:

    // precision
    if (*fmt == '.') {
        spec.flags |= SF_PREC;
        fmt++;
        if (*fmt == '*') {
            spec.precision = PREC_ARG;
            fmt++;
        } else {
            while (*fmt) {
                if (isdigit(*fmt)) {
                    spec.precision *= 10;
                    spec.precision += *fmt - '0';
                } else {
                    goto end_precision;
                }
                fmt++;
            }
        }
    }
end_precision:

    // length
    switch (*fmt) {
    case 'h':
        if (fmt[1] == 'h') {
            spec.type = BYTE;
            fmt++;
        } else {
            spec.type = SHORT;
        }
        break;
    case 'l':
        if (fmt[1] == 'l') {
            spec.type = LONG_LONG;
            fmt++;
        } else {
            spec.type = LONG;
        }
        break;
    case 'j':
        spec.type = INTMAX_T;
        break;
    case 'z':
        spec.type = SIZE_T;
        break;
    case 't':
        spec.type = PTRDIFF_T;
        break;
    default:
        spec.type = INT;
        goto end_length;
        break;
    }
    fmt++;
end_length:

    // specifier
    switch (*fmt) {
    case '%':
        spec.type = PERCENT;
        break;
    case 'd':
    case 'i':
        spec.base = 10;
        break;
    case 'b':
    case 'u':
    case 'o':
    case 'x':
        spec.flags |= SF_LOWER;
    case 'X':
        switch (spec.type) {
        case BYTE:
            spec.type = UBYTE;
            break;
        case SHORT:
            spec.type = USHORT;
            break;
        case INT:
            spec.type = UINT;
            break;
        case LONG:
            spec.type = ULONG;
            break;
        case LONG_LONG:
            spec.type = ULONG_LONG;
            break;
        default:
            break;
        }
        if (*fmt == 'u') {
            spec.base = 10;
        } else if (*fmt == 'o') {
            spec.base = 8;
        } else if (*fmt == 'b') {
            spec.base = 2;
        } else {
            spec.base = 16;
        }
        break;
    case 'c':
        spec.type = spec.type == INT ? CHAR : INVALID;
        break;
    case 's':
        spec.type = spec.type == INT ? STR : INVALID;
        break;
    case 'p':
        if (spec.type == INT) {
            if (spec.precision == 0) {
                spec.precision = sizeof(void *) * 2;
            }
            spec.type = PTR;
            spec.base = 16;
            fmt++;

            switch (*fmt) {
            case 'U':
                spec.ptr_format = PTR_FMT_UUID;
                break;
            case 'v':
                spec.ptr_format = PTR_FMT_VADDR_LOWER;
                break;
            case 'V':
                spec.ptr_format = PTR_FMT_VADDR_UPPER;
                break;
            case 'p':
                spec.ptr_format = PTR_FMT_PADDR_LOWER;
                break;
            case 'P':
                spec.ptr_format = PTR_FMT_PADDR_UPPER;
                break;
            case 'h':
                if (spec.width != WIDTH_AUTO) {
                    spec.ptr_format = PTR_FMT_HEX_BYTES;
                } else {
                    fmt--;
                }
                break;
            case 'u':
                switch (fmt[1]) {
                case 'c':
                    spec.ptr_format = PTR_FMT_UTF32_CHAR;
                    fmt++;
                    break;
                case 's':
                    spec.ptr_format = PTR_FMT_UTF32_STRING;
                    fmt++;
                    break;
                default:
                    fmt--;
                    break;
                }
                break;
            default:
                if (spec.width != WIDTH_AUTO && *fmt && fmt[1] == 'h') {
                    spec.ptr_format = PTR_FMT_HEX_BYTES;
                    spec.ptr_separator = *fmt;
                    fmt++;
                } else {
                    fmt--;
                }
                break;
            }
        } else {
            spec.type = INVALID;
        }
        break;
    case 'n':
        spec.flags |= SF_PTR;
        break;
    default:
        goto end_specifier;
    }
    fmt++;
end_specifier:

    spec.next = fmt;

    return spec;
}

static int print_char(int (*func)(void *, char), void *farg, struct fmt_spec spec, va_list *args)
{
    int left = 0;
    int char_cnt = 0;
    int width;
    char ch;

    // check flags
    left = !!(spec.flags & SF_LEFT);

    if (spec.width == WIDTH_AUTO) {
        // set width to 1 if zero
        width = 1;
    } else if (spec.width == WIDTH_ARG) {
        // get additional args
        width = va_arg(*args, int);
    } else {
        width = spec.width;
    }
    if (spec.precision == PREC_ARG) {
        spec.precision = va_arg(*args, wchar_t);
    }

    // get char
    ch = va_arg(*args, int);

    // print
    if (left) {
        if (func(farg, ch)) {
            return char_cnt;
        }
        char_cnt++;
        width--;
    }
    while (left ? width : width - 1) {
        if (func(farg, ' ')) {
            return char_cnt;
        }
        char_cnt++;
        width--;
    }
    if (!left) {
        if (func(farg, ch)) {
            return char_cnt;
        }
        char_cnt++;
        width--;
    }

    return char_cnt;
}

static int print_str(int (*func)(void *, char), void *farg, struct fmt_spec spec, va_list *args)
{
    int left = 0;
    int char_cnt = 0;
    int width = 0, prec = INT_MAX, slen = 0, pad_len = 0;
    const char *str;

    // check flags
    left = !!(spec.flags & SF_LEFT);

    // get additional args if neded
    if (spec.width == WIDTH_ARG) {
        width = va_arg(*args, int);
        width = MAX(width, slen);
    } else if (spec.width) {
        width = spec.width;
    }
    if (spec.precision == PREC_ARG) {
        prec = va_arg(*args, int);
    } else if (spec.precision) {
        prec = spec.precision;
    }

    // get str
    str = va_arg(*args, const char *);
    if (str == NULL) {
        str = "(null)";
    }

    slen = (int)strlen(str);

    if (width < slen) {
        width = slen;
    }
    if (prec < slen) {
        width = prec;
    }

    if (slen < width) {
        pad_len = width - slen;
    }

    // pad left
    if (!left) {
        while (pad_len > 0) {
            if (func(farg, ' ')) {
                return char_cnt;
            }
            char_cnt++;
            pad_len--;
            width--;
        }
    }

    while (width > 0 && *str) {
        if (func(farg, *str)) {
            return char_cnt;
        }
        str++;
        char_cnt++;
        width--;
    }

    // pad right
    if (left) {
        while (pad_len > 0) {
            if (func(farg, ' ')) {
                return char_cnt;
            }
            char_cnt++;
            pad_len--;
            width--;
        }
    }

    return char_cnt;
}

static const char hex_table_lower[] = "0123456789abcdef";
static const char hex_table_upper[] = "0123456789ABCDEF";

static int do_print_int(
    int (*func)(void *, char), void *farg, uintmax_t num, struct fmt_spec spec, int is_signed
)
{
    char rbuf[66], *rbuf_ptr = rbuf;  // buffer of reversed digits
    int rbuf_len = 0, char_cnt = 0;
    const char *hex_table = (spec.flags & SF_LOWER) ? hex_table_lower : hex_table_upper;
    char sign_char = (spec.flags & SF_SPACE) ? ' ' : '+';
    int left;
    int pad_len;

    if (is_signed && spec.base == 10 && (long long)num < 0) {
        num = -(long long)num;
        sign_char = '-';
    }

    if (spec.base == 2) {
        while (num) {
            *rbuf_ptr++ = (char)('0' + (num & 1));
            num >>= 1;
            rbuf_len++;
        }
    } else if (spec.base == 16) {
        while (num) {
            *rbuf_ptr++ = hex_table[num % 16];
            num /= 16;
            rbuf_len++;
        }
    } else if (spec.base == 8) {
        while (num) {
            *rbuf_ptr++ = (char)('0' + (num % 8));
            num /= 8;
            rbuf_len++;
        }
    } else {
        while (num) {
            *rbuf_ptr++ = (char)('0' + (num % 10));
            num /= 10;
            rbuf_len++;
        }
    }
    if (rbuf_len == 0) {
        *rbuf_ptr++ = '0';
        rbuf_len = 1;
    }

    left = !!(spec.flags & SF_LEFT);
    pad_len = spec.width - MAX(spec.precision, rbuf_len);
    if (spec.flags & SF_ZERO) {
        pad_len = 0;
        spec.precision = MAX(spec.width, spec.precision);
    }

    // print sign
    if ((sign_char == '-' || (spec.flags & (SF_SPACE | SF_PLUS)))) {
        if (func(farg, sign_char)) {
            return char_cnt;
        }
        pad_len--;
        char_cnt++;
    }

    // pad left
    if (!left) {
        while (pad_len > 0) {
            if (func(farg, ' ')) {
                return char_cnt;
            }
            pad_len--;
            char_cnt++;
        }
    }

    // pad zeroes
    while (spec.precision > rbuf_len) {
        if (func(farg, '0')) {
            return char_cnt;
        }
        spec.precision--;
        char_cnt++;
    }

    while (rbuf_len > 0) {  // now rewind reversed buffer
        if (func(farg, *--rbuf_ptr)) {
            return char_cnt;
        }
        rbuf_len--;
        char_cnt++;
    }

    // pad right
    if (left) {
        while (pad_len > 0) {
            if (func(farg, ' ')) {
                return char_cnt;
            }
            pad_len--;
            char_cnt++;
        }
    }

    return char_cnt;
}

static int print_literal(int (*func)(void *, char), void *farg, const char *str)
{
    int char_cnt = 0;

    while (*str) {
        if (func(farg, *str++)) {
            return char_cnt;
        }
        char_cnt++;
    }

    return char_cnt;
}

static int print_hex_byte(
    int (*func)(void *, char), void *farg, uint8_t byte, const char *hex_table
)
{
    int char_cnt = 0;

    if (func(farg, hex_table[byte >> 4])) {
        return char_cnt;
    }
    char_cnt++;

    if (func(farg, hex_table[byte & 0x0F])) {
        return char_cnt;
    }
    char_cnt++;

    return char_cnt;
}

static int print_uuid(int (*func)(void *, char), void *farg, const struct StUuid *uuid)
{
    static const uint8_t hyphen_after_byte[] = {3, 5, 7, 9};

    int char_cnt = 0;
    size_t hyphen_idx = 0;

    if (!uuid) {
        return print_literal(func, farg, "(null)");
    }

    for (size_t i = 0; i < sizeof(uuid->data); i++) {
        char_cnt += print_hex_byte(func, farg, uuid->data[i], hex_table_upper);

        if (hyphen_idx < ARRAY_SIZE(hyphen_after_byte) && i == hyphen_after_byte[hyphen_idx]) {
            if (func(farg, '-')) {
                return char_cnt;
            }
            char_cnt++;
            hyphen_idx++;
        }
    }

    return char_cnt;
}

static int print_utf8_codepoint(int (*func)(void *, char), void *farg, St_Utf32Char codepoint)
{
    uint32_t raw = (uint32_t)codepoint;
    char encoded[4];
    int encoded_len = 0;
    int char_cnt = 0;

    if (raw > UTF8_MAX_CODEPOINT || (raw >= 0xD800 && raw <= 0xDFFF)) {
        raw = (uint32_t)UTF_REPLACEMENT_CODEPOINT;
    }

    if (raw < 0x80) {
        encoded[0] = (char)raw;
        encoded_len = 1;
    } else if (raw < 0x800) {
        encoded[0] = (char)(0xC0 | (raw >> 6));
        encoded[1] = (char)(0x80 | (raw & 0x3F));
        encoded_len = 2;
    } else if (raw < 0x10000) {
        encoded[0] = (char)(0xE0 | (raw >> 12));
        encoded[1] = (char)(0x80 | ((raw >> 6) & 0x3F));
        encoded[2] = (char)(0x80 | (raw & 0x3F));
        encoded_len = 3;
    } else {
        encoded[0] = (char)(0xF0 | (raw >> 18));
        encoded[1] = (char)(0x80 | ((raw >> 12) & 0x3F));
        encoded[2] = (char)(0x80 | ((raw >> 6) & 0x3F));
        encoded[3] = (char)(0x80 | (raw & 0x3F));
        encoded_len = 4;
    }

    for (int i = 0; i < encoded_len; i++) {
        if (func(farg, encoded[i])) {
            return char_cnt;
        }
        char_cnt++;
    }

    return char_cnt;
}

static int print_utf32_char_ptr(
    int (*func)(void *, char), void *farg, const St_Utf32Char *codepoint
)
{
    if (!codepoint) {
        return print_literal(func, farg, "(null)");
    }

    return print_utf8_codepoint(func, farg, *codepoint);
}

static int print_utf32_string(
    int (*func)(void *, char), void *farg, const St_Utf32Char *str, int max_chars
)
{
    int char_cnt = 0;

    if (!str) {
        return print_literal(func, farg, "(null)");
    }

    for (int i = 0; (max_chars < 0 || i < max_chars) && (uint32_t)*str; i++) {
        char_cnt += print_utf8_codepoint(func, farg, *str++);
    }

    return char_cnt;
}

static int print_hex_bytes(
    int (*func)(void *, char), void *farg, const void *ptr, int byte_count, char separator
)
{
    const uint8_t *bytes = ptr;
    int char_cnt = 0;

    if (byte_count <= 0) {
        return 0;
    }
    if (!bytes) {
        return print_literal(func, farg, "(null)");
    }

    for (int i = 0; i < byte_count; i++) {
        if (separator && i > 0) {
            if (func(farg, separator)) {
                return char_cnt;
            }
            char_cnt++;
        }
        char_cnt += print_hex_byte(func, farg, bytes[i], hex_table_lower);
    }

    return char_cnt;
}

static int print_prefixed_address(
    int (*func)(void *, char), void *farg, char address_type, uintptr_t address, int upper
)
{
    struct fmt_spec spec = {0};
    int char_cnt = 0;

    spec.base = 16;
    spec.precision = sizeof(void *) * 2;
    if (!upper) {
        spec.flags |= SF_LOWER;
    }

    if (func(farg, address_type)) {
        return char_cnt;
    }
    char_cnt++;
    if (func(farg, ':')) {
        return char_cnt;
    }
    char_cnt++;
    if (func(farg, '0')) {
        return char_cnt;
    }
    char_cnt++;
    if (func(farg, 'x')) {
        return char_cnt;
    }
    char_cnt++;

    return char_cnt + do_print_int(func, farg, address, spec, 0);
}

static int print_int(int (*func)(void *, char), void *farg, struct fmt_spec spec, va_list *args)
{
    int is_signed = 0;
    int char_cnt = 0;
    uintmax_t num;

    // get number
    switch (spec.type) {
    case ULONG_LONG:
        num = (unsigned long long)va_arg(*args, unsigned long long);
        break;
    case LONG_LONG:
        num = (long long)va_arg(*args, long long);
        is_signed = 1;
        break;
    case ULONG:
        num = (unsigned long)va_arg(*args, unsigned long);
        break;
    case LONG:
        num = (long)va_arg(*args, long);
        is_signed = 1;
        break;
    case UBYTE:
        num = (unsigned char)va_arg(*args, unsigned int);
        break;
    case BYTE:
        num = (int)(char)va_arg(*args, int);
        is_signed = 1;
        break;
    case USHORT:
        num = (unsigned short)va_arg(*args, unsigned int);
        break;
    case SHORT:
        num = (short)va_arg(*args, int);
        is_signed = 1;
        break;
    case UINT:
        num = (unsigned int)va_arg(*args, unsigned int);
        break;
    case INT:
        num = (int)va_arg(*args, int);
        is_signed = 1;
        break;
    case UINTMAX_T:
        num = (uintmax_t)va_arg(*args, uintmax_t);
        break;
    case INTMAX_T:
        num = (intmax_t)va_arg(*args, intmax_t);
        is_signed = 1;
        break;
    case SIZE_T:
        num = (size_t)va_arg(*args, size_t);
        break;
    case PTRDIFF_T:
        num = (ptrdiff_t)va_arg(*args, ptrdiff_t);
        is_signed = 1;
        break;
    case PTR:
        num = (uintptr_t)va_arg(*args, void *);
        break;
    default:
        break;
    }

    // get additional args if neded
    if (spec.width == WIDTH_ARG) {
        spec.width = va_arg(*args, int);
    }
    if (spec.precision == PREC_ARG) {
        spec.precision = va_arg(*args, wchar_t);
    }

    // print prefix if needed
    if ((spec.flags & SF_PREFIX) && spec.base != 10) {
        if (func(farg, '0')) {
            return char_cnt;
        }
        char_cnt++;
        if (spec.width > 0) {
            spec.width--;
        }

        if (spec.base == 16 || spec.base == 2) {
            char prefix = spec.base == 16 ? ((spec.flags & SF_LOWER) ? 'x' : 'X') : 'b';

            if (func(farg, prefix)) {
                return char_cnt;
            }
            char_cnt++;
            if (spec.width > 0) {
                spec.width--;
            }
        }
    }

    // print
    char_cnt += do_print_int(func, farg, num, spec, is_signed);

    return char_cnt;
}

static void consume_dynamic_format_args(struct fmt_spec *spec, va_list *args)
{
    if (spec->width == WIDTH_ARG) {
        spec->width = va_arg(*args, int);
    }
    if (spec->precision == PREC_ARG) {
        spec->precision = va_arg(*args, int);
    }
}

static int print_ptr(int (*func)(void *, char), void *farg, struct fmt_spec spec, va_list *args)
{
    int has_width = spec.width != WIDTH_AUTO;
    int has_precision = !!(spec.flags & SF_PREC);

    if (spec.ptr_format == PTR_FMT_DEFAULT) {
        return print_int(func, farg, spec, args);
    }

    consume_dynamic_format_args(&spec, args);

    switch (spec.ptr_format) {
    case PTR_FMT_UUID:
        return print_uuid(func, farg, va_arg(*args, const void *));
    case PTR_FMT_UTF32_CHAR:
        return print_utf32_char_ptr(func, farg, va_arg(*args, const void *));
    case PTR_FMT_UTF32_STRING:
        return print_utf32_string(
            func,
            farg,
            va_arg(*args, const void *),
            has_precision ? spec.precision : (has_width ? spec.width : -1)
        );
    case PTR_FMT_HEX_BYTES:
        return print_hex_bytes(
            func,
            farg,
            va_arg(*args, const void *),
            spec.width,
            spec.ptr_separator
        );
    case PTR_FMT_VADDR_LOWER:
        return print_prefixed_address(func, farg, 'V', (uintptr_t)va_arg(*args, const void *), 0);
    case PTR_FMT_VADDR_UPPER:
        return print_prefixed_address(func, farg, 'V', (uintptr_t)va_arg(*args, const void *), 1);
    case PTR_FMT_PADDR_LOWER: {
        const uintptr_t *address = va_arg(*args, const void *);

        return print_prefixed_address(func, farg, 'P', address ? *address : 0, 0);
    }
    case PTR_FMT_PADDR_UPPER: {
        const uintptr_t *address = va_arg(*args, const void *);

        return print_prefixed_address(func, farg, 'P', address ? *address : 0, 1);
    }
    default:
        return 0;
    }
}

int vcprintf(int (*func)(void *, char), void *farg, const char *fmt, va_list args)
{
    int write_count = 0;
    struct fmt_spec spec = {0};

    va_list newargs;
    va_copy(newargs, args);

    while (*fmt) {
        int fmt_write_len = 0;

        if (*fmt != '%') {
            if (func(farg, *fmt)) {
                break;
            }
            fmt++;
            write_count++;
            continue;
        }

        fmt++;
        spec = decode_spec(fmt);

        switch (spec.type) {
        case PERCENT:
            fmt_write_len = func(farg, '%') ? 0 : 1;
            break;
        case CHAR:
            fmt_write_len = print_char(func, farg, spec, &newargs);
            break;
        case STR:
            fmt_write_len = print_str(func, farg, spec, &newargs);
            break;
        case PTR:
            fmt_write_len = print_ptr(func, farg, spec, &newargs);
            break;
        case ULONG_LONG:
        case LONG_LONG:
        case ULONG:
        case LONG:
        case UBYTE:
        case BYTE:
        case USHORT:
        case SHORT:
        case UINT:
        case INT:
        case UINTMAX_T:
        case INTMAX_T:
        case SIZE_T:
        case PTRDIFF_T:
            fmt_write_len = print_int(func, farg, spec, &newargs);
            break;
        default:  // invalid / unrecognized format specifier
            break;
        }
        write_count += fmt_write_len;
        fmt = spec.next;
    }

    func(farg, 0);

    va_end(newargs);

    return write_count;
}
