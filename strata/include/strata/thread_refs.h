#ifndef __STRATA_THREAD_REFS_H__
#define __STRATA_THREAD_REFS_H__

#include <strata/compiler.h>

struct StThread;
typedef struct StThread *StThread_StrongRef __ref_strong;
typedef struct StThread *StThread_WeakRef __ref_weak;
typedef struct StThread *StThread_BorrowedRef __ref_borrowed;
typedef struct StThread *StThread_InternalRef __ref_internal;

#endif  // __STRATA_THREAD_REFS_H__
