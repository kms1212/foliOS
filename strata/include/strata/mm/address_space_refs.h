#ifndef __STRATA_MM_ADDRESS_SPACE_REFS_H__
#define __STRATA_MM_ADDRESS_SPACE_REFS_H__

#include <strata/compiler.h>

struct StAddressSpace;
typedef struct StAddressSpace *StAddressSpace_StrongRef __ref_strong;
typedef struct StAddressSpace *StAddressSpace_WeakRef __ref_weak;
typedef struct StAddressSpace *StAddressSpace_BorrowedRef __ref_borrowed;
typedef struct StAddressSpace *StAddressSpace_InternalRef __ref_internal;

#endif  // __STRATA_MM_ADDRESS_SPACE_REFS_H__
