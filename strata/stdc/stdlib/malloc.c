#include <stdlib.h>

#include <strata/mm/pool.h>
#include <strata/status.h>

void *malloc(size_t size)
{
    StStatus status;
    void *ptr;

    status = StPool_Allocate(size, &ptr);
    if (!CHECK_SUCCESS(status)) return NULL;

    return ptr;
}

void *calloc(size_t num, size_t size)
{
    StStatus status;
    void *ptr;

    status = StPool_AllocateClear(num * size, &ptr);
    if (!CHECK_SUCCESS(status)) return NULL;

    return ptr;
}

void *realloc(void *ptr, size_t size)
{
    StStatus status;
    void *new_ptr;

    status = StPool_Reallocate(ptr, size, &new_ptr);
    if (!CHECK_SUCCESS(status)) return NULL;

    return new_ptr;
}

void free(void *ptr)
{
    StPool_Free(ptr);
}
