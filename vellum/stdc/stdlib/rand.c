#include <stdlib.h>

#include <stdint.h>

static uint32_t next = 1;

int rand(void)
{
    next = next * 1103515245 + 12345;
    return (uint32_t)(next >> 16) & RAND_MAX;
}

void srand(unsigned int seed)
{
    next = seed;
}