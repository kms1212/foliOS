#include <stdio.h>

struct cb_args {
    char *buf;
    size_t len;
};

static int write_buffer(void *_args, char ch)
{
    struct cb_args* args = _args;

    if (args->len < 1) {
        return 1;
    }
    
    *args->buf++ = ch;
    args->len--;

    return 0;
}

int vsnprintf(char *buf, size_t len, const char *fmt, va_list args)
{
    struct cb_args cb_args = {
        .buf = buf,
        .len = len,
    };
    return vcprintf(write_buffer, &cb_args, fmt, args);
}
