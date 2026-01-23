#include <stdio.h>

#include <string.h>

int fputs(const char *__restrict str, FILE *__restrict stream)
{
    fwrite(str, strlen(str), 1, stream);
    return 0;
}
