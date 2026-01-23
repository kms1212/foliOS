#include <stdio.h>

int freopencookie(void *cookie, const char *mode, cookie_io_functions_t io_funcs, FILE *stream)
{
    stream->type = 3;
    stream->cookie.cookie = cookie;
    stream->cookie.io_funcs = io_funcs;

    return 0;
}
