#ifndef __STRATA_MM_UTILS_H__
#define __STRATA_MM_UTILS_H__

#include <stddef.h>
#include <stdint.h>

#include <strata/mm/address_space_refs.h>

#include <strata/status.h>

/* Local -> Global (memcpy) */
StStatus StMm_ReadLocal(
    StAddressSpace_StrongRef asp __in, uintptr_t addr __in, void *buf __buf, size_t len __in
);

/* Global -> Local (memcpy) */
StStatus StMm_WriteLocal(
    StAddressSpace_StrongRef asp __in, uintptr_t addr __in, const void *buf __in, size_t len __in
);

/* Local (memset) */
StStatus StMm_SetLocal(
    StAddressSpace_StrongRef asp __in, uintptr_t addr __in, int value, size_t len __in
);

/* Local -> Local (memcpy) */
StStatus StMm_CopyLocal(
    StAddressSpace_StrongRef dest_asp __in,
    uintptr_t dest __in,
    StAddressSpace_StrongRef src_asp __in,
    uintptr_t src __in,
    size_t len __in
);

#endif  // __STRATA_MM_UTILS_H__
