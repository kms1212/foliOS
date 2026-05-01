#ifndef __STRATA_MM_POOL_H__
#define __STRATA_MM_POOL_H__

#include <stddef.h>

#include <strata/status.h>

StStatus StPool_Allocate(size_t size __in, void **ptr __out);
StStatus StPool_AllocateAligned(size_t size __in, int alignment __in, void **ptr __out);
StStatus StPool_AllocateClear(size_t size __in, void **ptr __out);
StStatus StPool_AllocateClearAligned(size_t size __in, int alignment __in, void **ptr __out);
StStatus StPool_Reallocate(void *ptr __in, size_t size __in, void **new_ptr __out);
StStatus StPool_Free(void *ptr __in);

#endif  // __STRATA_MM_POOL_H__
