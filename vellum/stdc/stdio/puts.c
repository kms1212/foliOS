#include <stdio.h>

#include <string.h>

int puts(const char *str)
{
    fwrite(str, strlen(str), 1, stdout);
    fputc('\n', stdout);
    return 0;
}
