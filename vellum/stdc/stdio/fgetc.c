#include <stdio.h>

int fgetc(FILE *stream)
{
    char ch;
    fread(&ch, 1, 1, stream);
    return ch;
}
