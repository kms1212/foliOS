#ifndef __STRATA_PROCESS_REFS_H__
#define __STRATA_PROCESS_REFS_H__

#include <strata/compiler.h>

struct StProcess;
typedef struct StProcess *StProcess_StrongRef __ref_strong;
typedef struct StProcess *StProcess_WeakRef __ref_weak;
typedef struct StProcess *StProcess_BorrowedRef __ref_borrowed;
typedef struct StProcess *StProcess_InternalRef __ref_internal;

#endif  // __STRATA_PROCESS_REFS_H__
