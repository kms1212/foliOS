#ifndef __STRATA_MM_OWNER_H__
#define __STRATA_MM_OWNER_H__

#include <stdint.h>

#include <strata/compiler.h>
#include <strata/status.h>
#include <strata/types.h>

#include <strata/mm/types.h>

struct StMm_AllocationOwner {
    void *first_vmm_node;
    void *last_vmm_node;
    St_PageCount page_usage_count;
    St_PageCount page_usage_peak_count;
};

#endif  // __STRATA_MM_OWNER_H__
