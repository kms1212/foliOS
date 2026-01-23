#include <stdio.h>

#undef vfprintf

static int write_file(void *arg, char ch)
{
    FILE* fp = arg;

    fwrite(&ch, 1, 1, fp);

    return 0;
}

int vfprintf(FILE *__restrict fp, const char *__restrict fmt, va_list args)
{
    return vcprintf(write_file, fp, fmt, args);
}
