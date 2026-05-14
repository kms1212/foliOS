#ifndef __STRATA_MM_ALLOCATION_OWNER_REFS_H__
#define __STRATA_MM_ALLOCATION_OWNER_REFS_H__

#include <strata/compiler.h>

struct StAllocationOwner;
typedef struct StAllocationOwner *StAllocationOwner_StrongRef __ref_strong;
typedef struct StAllocationOwner *StAllocationOwner_WeakRef __ref_weak;
typedef struct StAllocationOwner *StAllocationOwner_BorrowedRef __ref_borrowed;
typedef struct StAllocationOwner *StAllocationOwner_InternalRef __ref_internal;

#endif  // __STRATA_MM_ALLOCATION_OWNER_REFS_H__
