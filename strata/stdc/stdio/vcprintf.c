#include <stdio.h>

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <strata/macros.h>

#define SF_LEFT     0x01
#define SF_PLUS     0x02
#define SF_SPACE    0x04
#define SF_ZERO     0x08
#define SF_LOWER    0x10
#define SF_PREFIX   0x20
#define SF_PTR      0x40

#define WIDTH_AUTO  0
#define PREC_ARG    -1
#define WIDTH_ARG   -1

enum fmt_spec_type {
    INVALID = 0,
    PERCENT,
    WCHAR,
    CHAR,
    WSTR,
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
    DOUBLE,
    LONG_DOUBLE,
};

struct fmt_spec {
    short width;
    short precision;
    uint8_t flags;
    uint8_t base;
    uint8_t type;
    const char *next;
};

static struct fmt_spec decode_spec(const char *fmt)
{
    struct fmt_spec spec = { 0 };

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
        case 'L':
            spec.type = LONG_DOUBLE;
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
            if (spec.type == LONG_DOUBLE) {
                spec.type = INVALID;
            }
            spec.base = 10;
            break;
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
                case LONG_DOUBLE:
                    spec.type = INVALID;
                    break;
                default:
                    break;
            }
            if (*fmt == 'u') {
                spec.base = 10;
            } else if (*fmt == 'o') {
                spec.base = 8;
            } else {
                spec.base = 16;
            }
            break;
        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
        case 'a':
        case 'A':
            switch (spec.type) {
                case INT:
                    spec.type = DOUBLE;
                    break;
                case LONG_DOUBLE:
                    break;
                default:
                    spec.type = INVALID;
            }
            break;
        case 'c':
            switch (spec.type) {
                case INT:
                    spec.type = CHAR;
                    break;
                case LONG:
                    spec.type = WCHAR;
                    break;
                default:
                    spec.type = INVALID;
            }
            break;
        case 's':
            switch (spec.type) {
                case INT:
                    spec.type = STR;
                    break;
                case LONG:
                    spec.type = WSTR;
                    break;
                default:
                    spec.type = INVALID;
            }
            break;
        case 'p':
            if (spec.type == INT) {
                if (spec.precision == 0) {
                    spec.precision = sizeof(void*) * 2;
                }
                spec.type = PTR;
                spec.base = 16;
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

static int print_wchar(int (*func)(void *, char), void *farg, struct fmt_spec spec, va_list *args)
{
    // we do not print wchar
    va_arg(*args, wchar_t);

    if (spec.width == WIDTH_ARG) {
        va_arg(*args, int);
    }

    if (spec.precision == PREC_ARG) {
        va_arg(*args, int);
    }
    return 0;
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

static int print_wstr(int (*func)(void *, char), void *farg, struct fmt_spec spec, va_list *args)
{
    // we do not print wstr
    va_arg(*args, wchar_t *);

    if (spec.width == WIDTH_ARG) {
        va_arg(*args, int);
    }

    if (spec.precision == PREC_ARG) {
        va_arg(*args, int);
    }
    return 0;
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
    }
    if (spec.precision == PREC_ARG) {
        prec = va_arg(*args, int);
    }

    // get str
    str = va_arg(*args, const char *);
    if (str == NULL) {
        str = "(null)";
    }

    slen = strlen(str);

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

static int do_print_int(int (*func)(void *, char), void *farg, unsigned long long num, struct fmt_spec spec, int is_signed)
{
    char rbuf[22], *rbuf_ptr = rbuf;  // buffer of reversed digits
    int rbuf_len = 0, char_cnt = 0;
    const char *hex_table = (spec.flags & SF_LOWER) ? hex_table_lower : hex_table_upper;
    char sign_char = (spec.flags & SF_SPACE) ? ' ' : '+';
    int left;
    int pad_len;

    if (is_signed && spec.base == 10 && (long long)num < 0) {
        num = -(long long)num;
        sign_char = '-';
    }

    while (num) {
        *rbuf_ptr++ = hex_table[num % spec.base];
        num /= spec.base;
        rbuf_len++;
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

static int print_int(int (*func)(void *, char), void *farg, struct fmt_spec spec, va_list *args)
{
    int is_signed = 0;
    int char_cnt = 0;
    unsigned long long num;

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
            num = (char)va_arg(*args, int);
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
            num = (unsigned long)va_arg(*args, void*);
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
        if (spec.width > 0) {
            if (func(farg, '0')) {
                return char_cnt;
            }
            spec.width--;
            char_cnt++;
        }
        if (spec.width > 0 && spec.base == 16) {
            if (func(farg, (spec.flags & SF_LOWER) ? 'x' : 'X')) {
                return char_cnt;
            }
            spec.width--;
            char_cnt++;
        }
    }

    // print
    char_cnt += do_print_int(func, farg, num, spec, is_signed);

    return char_cnt;
}

static int print_float(int (*func)(void *, char), void *farg, struct fmt_spec spec, va_list *args)
{
    // we do not print float
    if (spec.type == DOUBLE) {
        va_arg(*args, double);
    } else if (spec.type == LONG_DOUBLE) {
        va_arg(*args, long double);
    }

    if (spec.width == WIDTH_ARG) {
        va_arg(*args, int);
    }

    if (spec.precision == PREC_ARG) {
        va_arg(*args, int);
    }
    return 0;
}

int vcprintf(int (*func)(void *, char), void *farg, const char *fmt, va_list args)
{
    int write_count = 0;
    struct fmt_spec spec = { 0 };

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
            case WCHAR:
                fmt_write_len = print_wchar(func, farg, spec, &newargs);
                break;
            case CHAR:
                fmt_write_len = print_char(func, farg, spec, &newargs);
                break;
            case WSTR:
                fmt_write_len = print_wstr(func, farg, spec, &newargs);
                break;
            case STR:
                fmt_write_len = print_str(func, farg, spec, &newargs);
                break;
            case PTR:
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
            case DOUBLE:
            case LONG_DOUBLE:
                fmt_write_len = print_float(func, farg, spec, &newargs);
                break;
            default:  // invalid / unrecognized format specifier
                break;
        }
        write_count += fmt_write_len;
        fmt = spec.next;
    }

    func(farg, 0);

    return write_count;
}
