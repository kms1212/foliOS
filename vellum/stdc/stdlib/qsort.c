#include <stdlib.h>

#include <stdint.h>

static void swap(void *_a, void *_b, size_t size)
{
    uint8_t *a = _a, *b = _b, temp;
    for (size_t i = 0; i < size; i++) {
        temp = *a;
        *a = *b;
        *b = temp;
        a++;
        b++;
    }
}

void qsort(void *base, size_t num, size_t size, int (*cmp)(const void*, const void*))
{
    return;
}
