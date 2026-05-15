#ifndef __MM_INTERNAL_H__
#define __MM_INTERNAL_H__

#include <stdatomic.h>
#include <stdint.h>

#include <strata/mm/allocation_owner.h>
#include <strata/mm/pmm.h>
#include <strata/mm/types.h>
#include <strata/mm/vmm.h>

struct pmm_metadata_ipublic_view {
    uint32_t order;
    uint32_t flags;
    StAllocationOwner_StrongRef owner;
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

#define AF_VMM_RESERVATION_MAP       ((StMm_AllocFlags)0x10000000)
#define AF_VMM_RESERVATION_SPARSE    ((StMm_AllocFlags)0x20000000)
#define AF_VMM_RESERVATION_ON_DEMAND ((StMm_AllocFlags)0x40000000)
#define AF_VMM_RESERVATION_MASK                                                                  \
    (AF_VMM_RESERVATION_MAP | AF_VMM_RESERVATION_SPARSE | AF_VMM_RESERVATION_ON_DEMAND)

struct vmm_reservation_domain {
    struct vmm_reservation_node *head;
    St_VirtPage base_vpn, limit_vpn;
    St_PageCount free_count;
    int initialized;
};

#define VMM_RESERVATION_ALLOC 0
#define VMM_RESERVATION_MAP   1

struct vmm_reservation_node {
    St_VirtPage base_vpn, limit_vpn;
    StAllocationOwner_StrongRef owner;
    struct vmm_reservation_node *owner_prev, *owner_next;
    struct vmm_reservation_node *domain_prev, *domain_next;
    StAddressSpace_InternalRef asp;
    uint32_t reservation_type;
    St_PageCount guard_page_count;
    StMm_AllocFlags alloc_flags;
    StMm_MapFlags map_flags;
    struct StVmm_PageMappingPolicy mapping_policy;
    enum StVmm_Domain domain;
    uint8_t is_live;
};

#endif  // __MM_INTERNAL_H__
