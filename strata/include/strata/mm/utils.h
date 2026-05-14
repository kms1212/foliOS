#ifndef __STRATA_MM_UTILS_H__
#define __STRATA_MM_UTILS_H__

#include <strata/mm/asp.h>

#include <strata/status.h>

/* Local -> Global (memcpy) */
StStatus StMm_ReadLocal(
    StMm_AddressSpace_StrongRef asp __in, uintptr_t addr __in, void *buf __buf, size_t len __in
);

/* Global -> Local (memcpy) */
StStatus StMm_WriteLocal(
    StMm_AddressSpace_StrongRef asp __in, uintptr_t addr __in, const void *buf __in, size_t len __in
);

/* Local (memset)*/
StStatus StMm_SetLocal(
    StMm_AddressSpace_StrongRef asp __in, uintptr_t addr __in, int value, size_t len __in
);

/* Local -> Local (memcpy) */
StStatus StMm_CopyLocal(
    StMm_AddressSpace_StrongRef dest_asp __in,
    uintptr_t dest __in,
    StMm_AddressSpace_StrongRef src_asp __in,
    uintptr_t src __in,
    size_t len __in
);

#endif  // __STRATA_MM_UTILS_H__
