#ifndef __STRATA_MM_UTILS_H__
#define __STRATA_MM_UTILS_H__

#include <strata/mm/asp.h>

#include <strata/status.h>

/* Local -> Global (memcpy) */
StStatus StMm_ReadLocal(
    struct StMm_AddressSpace *asp __in, uintptr_t addr __in, void *buf __buf, size_t len __in
);

/* Global -> Local (memcpy) */
StStatus StMm_WriteLocal(
    struct StMm_AddressSpace *asp __in, uintptr_t addr __in, const void *buf __in, size_t len __in
);

/* Local (memset)*/
StStatus StMm_SetLocal(
    struct StMm_AddressSpace *asp __in, uintptr_t addr __in, int value, size_t len __in
);

/* Local -> Local (memcpy) */
StStatus StMm_CopyLocal(
    struct StMm_AddressSpace *dest_asp __in,
    uintptr_t dest __in,
    struct StMm_AddressSpace *src_asp __in,
    uintptr_t src __in,
    size_t len __in
);

#endif  // __STRATA_MM_UTILS_H__
