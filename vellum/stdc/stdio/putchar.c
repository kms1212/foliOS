#include <stdio.h>

int putchar(int ch)
{
    fwrite(&ch, 1, 1, stdout);
    return 0;
}
