#ifndef __STRATA_MM_PMM_REFS_H__
#define __STRATA_MM_PMM_REFS_H__

#include <strata/compiler.h>

struct StPmm_AllocationMetadata;
typedef struct StPmm_AllocationMetadata *StPmm_AllocationMetadata_BorrowedRef __ref_borrowed;
typedef struct StPmm_AllocationMetadata *StPmm_AllocationMetadata_LockedRef __ref_locked;

#endif  // __STRATA_MM_PMM_REFS_H__
