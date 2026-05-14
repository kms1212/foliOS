#ifndef __STRATA_GNT_REFS_H__
#define __STRATA_GNT_REFS_H__

#include <strata/compiler.h>

struct StGnt_Node;
typedef struct StGnt_Node *StGnt_Node_StrongRef __ref_strong;
typedef struct StGnt_Node *StGnt_Node_WeakRef __ref_weak;
typedef struct StGnt_Node *StGnt_Node_BorrowedRef __ref_borrowed;
typedef struct StGnt_Node *StGnt_Node_InternalRef __ref_internal;

#endif  // __STRATA_GNT_REFS_H__
