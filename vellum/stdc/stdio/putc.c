#include <stdio.h>

int putc(int ch, FILE *stream)
{
    fwrite(&ch, 1, 1, stream);
    return 0;
}
