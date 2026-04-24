#ifndef __MM_INTERNAL_H__
#define __MM_INTERNAL_H__

#include <stdatomic.h>
#include <stdint.h>

#include <strata/rb.h>

#include <strata/mm/owner.h>
#include <strata/mm/pmm.h>
#include <strata/mm/types.h>
#include <strata/mm/vmm.h>

struct pmm_metadata_ipublic_view {
    uint32_t order;
    uint32_t flags;
    struct StMm_AllocationOwner *owner;
};

_Static_assert(
    sizeof(struct StPmm_AllocationMetadata) == sizeof(struct pmm_metadata_ipublic_view),
    "public pmm metadata struct size mismatch"
);

struct pmm_metadata {
    struct pmm_metadata_ipublic_view public;

    atomic_uint refcount;
    atomic_uint lock;

    uint8_t padding[64 - sizeof(struct pmm_metadata_ipublic_view) - (sizeof(atomic_uint) * 2)];
} __aligned(64);

_Static_assert(
    sizeof(struct pmm_metadata) == 64,
    "pmm metadata struct size mismatch (sizeof(struct pmm_metadata) != 64)"
);

#define AF_VMM_HIDDEN_AT_MAP ((StMm_MapFlags)0x10000000)

struct vmm_alloc_domain {
    struct StRbtree rbtree;
    St_VirtPage base_vpn, limit_vpn;
    St_PageCount free_count;
    int initialized;
};

#define AT_ALLOC 0
#define AT_MAP   1

struct vmm_alloc_node {
    struct StRbtree_Node rbnode;
    St_VirtPage base_vpn, limit_vpn;
    struct StMm_AllocationOwner *owner;
    struct vmm_alloc_node *owner_prev, *owner_next;
    struct StMm_AddressSpace *asp;
    uint32_t alloc_type;
    enum StVmm_Domain domain;
};

#endif  // __MM_INTERNAL_H__
