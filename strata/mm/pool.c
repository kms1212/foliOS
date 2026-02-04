#include <strata/mm/pool.h>

#include <liballoc.h>
#include <strata/status.h>

StStatus StPool_Allocate(size_t size __in, void **ptr __out)
{
    *ptr = la_malloc(size);
    return *ptr ? STATUS_SUCCESS : STATUS_UNKNOWN_ERROR;
}

StStatus StPool_AllocateAligned(size_t size __in, int align __in, void **ptr __out)
{
    return STATUS_UNIMPLEMENTED;
}

StStatus StPool_AllocateClear(size_t size __in, void **ptr __out)
{
    *ptr = la_calloc(1, size);
    return *ptr ? STATUS_SUCCESS : STATUS_UNKNOWN_ERROR;
}

StStatus StPool_AllocateClearAligned(size_t size __in, int align __in, void **ptr __out)
{
    return STATUS_UNIMPLEMENTED;
}

StStatus StPool_Reallocate(void *ptr __in, size_t size __in, void **new_ptr __out)
{
    *new_ptr = la_realloc(ptr, size);
    return *new_ptr ? STATUS_SUCCESS : STATUS_UNKNOWN_ERROR;
}

StStatus StPool_Free(void *ptr __in)
{
    la_free(ptr);
    return STATUS_SUCCESS;
}
