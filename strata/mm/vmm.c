#include <strata/mm/vmm.h>

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <strata/arch/interrupt.h>
#include <strata/arch/mmu_constants.h>

#include <strata/compiler.h>
#include <strata/log.h>
#include <strata/mm.h>
#include <strata/mm/address_space_refs.h>
#include <strata/mm/address_space.h>
#include <strata/mm/allocation_owner.h>
#include <strata/mm/allocation_owner_refs.h>
#include <strata/mm/pmm.h>
#include <strata/mm/types.h>
#include <strata/plat/memmap.h>
#include <strata/process.h>
#include <strata/status.h>
#include <strata/thread.h>
#include <strata/types.h>

#include "internal.h"

#define MODULE_NAME "vmm"

#define EARLY_ALLOC_NODE_POOL_COUNT      1024
#define DYNAMIC_ALLOC_NODE_LOW_WATERMARK 16
#define DYNAMIC_ALLOC_NODE_MAX_SLABS     256

static struct vmm_alloc_domain alloc_domain_list[VMM_DOMAIN_MAX] = {
    [VMM_DOMAIN_MODULE] = {.initialized = 0},
    [VMM_DOMAIN_KERNEL_FAST] = {.initialized = 0},
    [VMM_DOMAIN_KERNEL_SLOW] = {.initialized = 0},
    [VMM_DOMAIN_IO] = {.initialized = 0},
    [VMM_DOMAIN_KRT_GLOBAL] = {.initialized = 0},
};

static struct vmm_alloc_node early_alloc_node_pool[EARLY_ALLOC_NODE_POOL_COUNT];
static int early_alloc_node_pool_used_count = 0;

static struct vmm_alloc_node *dynamic_alloc_node_freelist = NULL;
static size_t dynamic_alloc_node_free_count = 0;
static size_t dynamic_alloc_node_total_count = 0;
static int is_topping_up_dynamic_node_pool = 0;

static struct {
    uintptr_t start;
    uintptr_t end;
} dynamic_alloc_node_slabs[DYNAMIC_ALLOC_NODE_MAX_SLABS];
static size_t dynamic_alloc_node_slab_count = 0;
static uint32_t validate_fail_log_budget = 32;

#define PHYS_TO_DIRECTMAP_PTR(pa)                                                                  \
    ((void *)((uintptr_t)(pa) + PAGE_TO_ADDR(MEMMAP_DIRECTMAP_VPN_BASE)))

static int is_node_in_dynamic_pool(const struct vmm_alloc_node *node)
{
    uintptr_t addr;

    if (!node) return 0;
    addr = (uintptr_t)node;

    for (size_t i = 0; i < dynamic_alloc_node_slab_count; i++) {
        uintptr_t start = dynamic_alloc_node_slabs[i].start;
        uintptr_t end = dynamic_alloc_node_slabs[i].end;
        if (addr >= start && addr < end) {
            if ((addr - start) % sizeof(struct vmm_alloc_node) != 0) return 0;
            return 1;
        }
    }

    return 0;
}

static void push_dynamic_alloc_node(struct vmm_alloc_node *node)
{
    node->domain_next = dynamic_alloc_node_freelist;
    dynamic_alloc_node_freelist = node;
    dynamic_alloc_node_free_count++;
}

static struct vmm_alloc_node *pop_dynamic_alloc_node(void)
{
    struct vmm_alloc_node *node = dynamic_alloc_node_freelist;
    if (!node) return NULL;

    dynamic_alloc_node_freelist = node->domain_next;
    node->domain_next = NULL;
    dynamic_alloc_node_free_count--;

    return node;
}

static StStatus topup_dynamic_alloc_node_pool(void)
{
    StStatus status;
    St_PhysFrame pfn = (St_PhysFrame)-1;
    struct vmm_alloc_node *base;
    size_t node_count;

    if (is_topping_up_dynamic_node_pool) return STATUS_SUCCESS;
    if (dynamic_alloc_node_slab_count >= DYNAMIC_ALLOC_NODE_MAX_SLABS) {
        return STATUS_TOO_LARGE;
    }

    is_topping_up_dynamic_node_pool = 1;

    status = StPmm_AllocateContiguousFrame(&pfn, (St_PageCount)1, NULL, AF_DEFAULT);
    if (!CHECK_SUCCESS(status)) {
        is_topping_up_dynamic_node_pool = 0;
        return status;
    }

    base = PHYS_TO_DIRECTMAP_PTR(FRAME_TO_ADDR(pfn));
    node_count = PAGE_SIZE / sizeof(struct vmm_alloc_node);

    dynamic_alloc_node_slabs[dynamic_alloc_node_slab_count].start = (uintptr_t)base;
    dynamic_alloc_node_slabs[dynamic_alloc_node_slab_count].end = (uintptr_t)base + PAGE_SIZE;
    dynamic_alloc_node_slab_count++;

    for (size_t i = 0; i < node_count; i++) {
        memset(&base[i], 0, sizeof(base[i]));
        push_dynamic_alloc_node(&base[i]);
    }
    dynamic_alloc_node_total_count += node_count;

    is_topping_up_dynamic_node_pool = 0;
    return STATUS_SUCCESS;
}

static void maybe_topup_dynamic_alloc_node_pool(void)
{
    if (is_topping_up_dynamic_node_pool) return;
    if (dynamic_alloc_node_free_count > DYNAMIC_ALLOC_NODE_LOW_WATERMARK) return;
    (void)topup_dynamic_alloc_node_pool();
}

static void release_alloc_node(struct vmm_alloc_node *node)
{
    if (!node) return;
    node->is_live = 0;
    if (is_node_in_dynamic_pool(node)) {
        push_dynamic_alloc_node(node);
    }
}

static int is_node_in_pool(const struct vmm_alloc_node *node)
{
    uintptr_t addr;
    uintptr_t start;
    uintptr_t end;

    if (!node) return 0;

    addr = (uintptr_t)node;
    start = (uintptr_t)&early_alloc_node_pool[0];
    end = (uintptr_t)&early_alloc_node_pool[EARLY_ALLOC_NODE_POOL_COUNT];

    if (addr >= start && addr < end && ((addr - start) % sizeof(early_alloc_node_pool[0]) == 0)) {
        return 1;
    }

    return is_node_in_dynamic_pool(node);
}

static void log_validate_failure(
    const char *where,
    const char *reason,
    struct vmm_alloc_node *head,
    struct vmm_alloc_node *prev,
    struct vmm_alloc_node *curr,
    size_t guard
)
{
    if (!validate_fail_log_budget) return;
    validate_fail_log_budget--;

    LOG_ERROR(
        LM_CAT_UNCLASSIFIED,
        "vmm list invalid (%s): %s head=%013zX prev=%013zX curr=%013zX guard=%zu early_used=%d "
        "dyn_total=%zu dyn_free=%zu\n",
        where ? where : "?",
        reason ? reason : "?",
        (uintptr_t)head,
        (uintptr_t)prev,
        (uintptr_t)curr,
        guard,
        early_alloc_node_pool_used_count,
        dynamic_alloc_node_total_count,
        dynamic_alloc_node_free_count
    );
}

static int validate_domain_list(struct vmm_alloc_node *head, const char *where)
{
    struct vmm_alloc_node *prev = NULL;
    struct vmm_alloc_node *curr = head;
    size_t guard = (size_t)early_alloc_node_pool_used_count + dynamic_alloc_node_total_count + 1;

    while (curr) {
        if (!guard--) {
            log_validate_failure(where, "guard exhausted", head, prev, curr, guard);
            return 0;
        }
        if (!is_node_in_pool(curr)) {
            log_validate_failure(where, "curr not in pool", head, prev, curr, guard);
            return 0;
        }
        if (!curr->is_live) {
            log_validate_failure(where, "curr not live", head, prev, curr, guard);
            return 0;
        }
        if (curr->domain_prev != prev) {
            log_validate_failure(where, "broken prev link", head, prev, curr, guard);
            return 0;
        }
        if (curr->limit_vpn <= curr->base_vpn) {
            log_validate_failure(where, "invalid range", head, prev, curr, guard);
            return 0;
        }
        if (prev && prev->base_vpn >= curr->base_vpn) {
            log_validate_failure(where, "non-ascending base_vpn", head, prev, curr, guard);
            return 0;
        }
        if (curr->domain_next && !is_node_in_pool(curr->domain_next)) {
            log_validate_failure(where, "next not in pool", head, prev, curr, guard);
            return 0;
        }

        prev = curr;
        curr = curr->domain_next;
    }

    return 1;
}

static inline struct vmm_alloc_node **get_local_head_slot(StAddressSpace_StrongRef asp)
{
    return (struct vmm_alloc_node **)&asp->user_alloc_head;
}

static StStatus make_limit_exclusive(
    St_VirtPage base_vpn __in, St_PageCount count __in, St_VirtPage *limit_out __out
)
{
    assert(limit_out);

    St_VirtPage limit;

    if (count == 0) return STATUS_INVALID_VALUE;

    limit = base_vpn + (St_VirtPage)count;
    if (limit < base_vpn) return STATUS_INVALID_VALUE;

    *limit_out = limit;

    return STATUS_SUCCESS;
}

static StStatus make_domain_limit_exclusive(
    St_VirtPage limit_inclusive __in, St_VirtPage *limit_out __out
)
{
    assert(limit_out);

    if (limit_inclusive == (St_VirtPage)-1) return STATUS_CONFLICTING_STATE;

    *limit_out = limit_inclusive + 1;

    return STATUS_SUCCESS;
}

static StStatus create_alloc_node(struct vmm_alloc_node **node)
{
    int i;
    struct vmm_alloc_node *dynamic_node = NULL;

    for (i = 0; i < early_alloc_node_pool_used_count; i++) {
        if (!early_alloc_node_pool[i].is_live) {
            if (node) {
                *node = &early_alloc_node_pool[i];
                memset(*node, 0, sizeof(**node));
            }
            return STATUS_SUCCESS;
        }
    }

    if (early_alloc_node_pool_used_count < EARLY_ALLOC_NODE_POOL_COUNT) {
        if (node) {
            *node = &early_alloc_node_pool[early_alloc_node_pool_used_count];
            memset(*node, 0, sizeof(**node));
        }
        early_alloc_node_pool_used_count++;
        return STATUS_SUCCESS;
    }

    maybe_topup_dynamic_alloc_node_pool();

    if (!dynamic_alloc_node_free_count) {
        StStatus status = topup_dynamic_alloc_node_pool();
        if (!CHECK_SUCCESS(status) && status != STATUS_CONFLICTING_STATE) {
            LOG_ERROR(
                LM_CAT_UNCLASSIFIED,
                "vmm node pool exhausted: early_used=%d dyn_free=%zu dyn_total=%zu status=%08X\n",
                early_alloc_node_pool_used_count,
                dynamic_alloc_node_free_count,
                dynamic_alloc_node_total_count,
                status
            );
        }
    }

    dynamic_node = pop_dynamic_alloc_node();
    if (!dynamic_node) {
        LOG_ERROR(
            LM_CAT_UNCLASSIFIED,
            "vmm node allocation failed: early_used=%d dyn_free=%zu dyn_total=%zu\n",
            early_alloc_node_pool_used_count,
            dynamic_alloc_node_free_count,
            dynamic_alloc_node_total_count
        );
        return STATUS_INSUFFICIENT_MEMORY;
    }

    if (node) {
        *node = dynamic_node;
        memset(*node, 0, sizeof(**node));
    }

    return STATUS_SUCCESS;
}

static void init_alloc_node_metadata(
    struct vmm_alloc_node *node __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in,
    enum StVmm_BackingType backing_type __in
)
{
    node->alloc_type = (alloc_flags & AF_VMM_HIDDEN_AT_MAP) ? AT_MAP : AT_ALLOC;
    node->backing_type = backing_type;
    node->alloc_flags = alloc_flags;
    node->map_flags = map_flags;
}

static enum StVmm_BackingType local_backing_type_from_map_flags(StMm_MapFlags map_flags __in)
{
    if ((map_flags & MF_ZERO_FILL) && !(map_flags & MF_IMMEDIATE)) {
        return VMM_BACKING_DEMAND_ZERO;
    }

    return VMM_BACKING_PHYSICAL;
}

static StStatus attach_owner_node(struct vmm_alloc_node *node, StAllocationOwner_StrongRef owner)
{
    struct vmm_alloc_node *owner_last;

    node->owner = NULL;
    node->owner_prev = NULL;
    node->owner_next = NULL;

    if (!owner) return STATUS_SUCCESS;
    if (StAllocationOwner_IsClosed(owner)) return STATUS_CONFLICTING_STATE;

    StAllocationOwner_Acquire(owner);
    node->owner = owner;

    owner_last = (struct vmm_alloc_node *)owner->last_vmm_node;
    node->owner_prev = owner_last;
    if (owner_last) {
        owner_last->owner_next = node;
    } else {
        owner->first_vmm_node = node;
    }
    owner->last_vmm_node = node;

    return STATUS_SUCCESS;
}

static void detach_owner_node(struct vmm_alloc_node *node)
{
    StAllocationOwner_StrongRef owner;

    if (!node || !node->owner) return;

    owner = node->owner;

    if (node->owner_prev) {
        node->owner_prev->owner_next = node->owner_next;
    } else {
        owner->first_vmm_node = node->owner_next;
    }

    if (node->owner_next) {
        node->owner_next->owner_prev = node->owner_prev;
    } else {
        owner->last_vmm_node = node->owner_prev;
    }

    node->owner = NULL;
    node->owner_prev = NULL;
    node->owner_next = NULL;

    StAllocationOwner_Release(owner);
}

static StStatus insert_domain_node_sorted(
    struct vmm_alloc_node **head_slot __inout, struct vmm_alloc_node *node __in
)
{
    assert(head_slot);

    struct vmm_alloc_node *prev = NULL;
    struct vmm_alloc_node *curr = *head_slot;

    while (curr && curr->base_vpn < node->base_vpn) {
        prev = curr;
        curr = curr->domain_next;
    }

    if (curr && curr->base_vpn == node->base_vpn) {
        return STATUS_DUPLICATE_ENTRY;
    }

    node->domain_prev = prev;
    node->domain_next = curr;

    if (prev) {
        prev->domain_next = node;
    } else {
        *head_slot = node;
    }

    if (curr) curr->domain_prev = node;

    return STATUS_SUCCESS;
}

static void remove_domain_node(
    struct vmm_alloc_node **head_slot __inout, struct vmm_alloc_node *node __in
)
{
    assert(head_slot);

    if (node->domain_prev) {
        node->domain_prev->domain_next = node->domain_next;
    } else if (*head_slot == node) {
        *head_slot = node->domain_next;
    }

    if (node->domain_next) node->domain_next->domain_prev = node->domain_prev;

    node->domain_prev = NULL;
    node->domain_next = NULL;
}

static struct vmm_alloc_node *find_overlap(
    struct vmm_alloc_node *head __in, St_VirtPage base_vpn __in, St_PageCount count __in
)
{
    struct vmm_alloc_node *node = head;
    St_VirtPage limit_vpn;

    if (!CHECK_SUCCESS(make_limit_exclusive(base_vpn, count, &limit_vpn))) return NULL;

    while (node) {
        if (node->base_vpn >= limit_vpn) break;

        if (base_vpn < node->limit_vpn && node->base_vpn < limit_vpn) return node;

        node = node->domain_next;
    }

    return NULL;
}

static struct vmm_alloc_node *find_exact(
    struct vmm_alloc_node *head __in, St_VirtPage base_vpn __in, St_PageCount count __in
)
{
    struct vmm_alloc_node *node = head;
    St_VirtPage limit_vpn;

    if (!CHECK_SUCCESS(make_limit_exclusive(base_vpn, count, &limit_vpn))) return NULL;

    while (node) {
        if (node->base_vpn == base_vpn) {
            if (node->limit_vpn == limit_vpn) return node;
            return NULL;
        }
        if (node->base_vpn > base_vpn) return NULL;
        node = node->domain_next;
    }

    return NULL;
}

static StStatus find_first_fit_from(
    struct vmm_alloc_node *head __in,
    St_VirtPage domain_base_vpn __in,
    St_VirtPage domain_limit_vpn __in,
    St_PageCount count __in,
    St_VirtPage search_start __in,
    St_VirtPage *result_vpn __out
)
{
    assert(result_vpn);

    struct vmm_alloc_node *curr = head;
    St_VirtPage candidate_start = domain_base_vpn;
    St_VirtPage domain_limit_exclusive;
    StStatus status;

    if (count == 0) return STATUS_INVALID_VALUE;

    status = make_domain_limit_exclusive(domain_limit_vpn, &domain_limit_exclusive);
    if (!CHECK_SUCCESS(status)) return status;

    if (search_start > candidate_start) candidate_start = search_start;
    if (candidate_start >= domain_limit_exclusive) return STATUS_INSUFFICIENT_MEMORY;

    while (curr) {
        if (curr->limit_vpn <= candidate_start) {
            curr = curr->domain_next;
            continue;
        }

        if (curr->base_vpn > candidate_start) {
            St_PageCount gap = (St_PageCount)(curr->base_vpn - candidate_start);
            if (gap >= count) {
                *result_vpn = candidate_start;
                return STATUS_SUCCESS;
            }
        }

        if (curr->limit_vpn > candidate_start) candidate_start = curr->limit_vpn;
        if (candidate_start >= domain_limit_exclusive) return STATUS_INSUFFICIENT_MEMORY;

        curr = curr->domain_next;
    }

    if (domain_limit_exclusive > candidate_start) {
        St_PageCount gap = (St_PageCount)(domain_limit_exclusive - candidate_start);
        if (gap >= count) {
            *result_vpn = candidate_start;
            return STATUS_SUCCESS;
        }
    }

    return STATUS_INSUFFICIENT_MEMORY;
}

static int is_global_range_unmapped(St_VirtPage base_vpn __in, St_PageCount count __in)
{
    StStatus status;
    St_PageCount i;

    for (i = 0; i < count; i++) {
        status = StMm_GlobalVirtPageToPhysFrame(base_vpn + i, NULL);
        if (status != STATUS_PAGE_NOT_PRESENT) return 0;
    }

    return 1;
}

static int is_local_range_unmapped(
    StAddressSpace_StrongRef asp __in, St_VirtPage base_vpn __in, St_PageCount count __in
)
{
    StStatus status;
    St_PageCount i;

    for (i = 0; i < count; i++) {
        status = StMm_LocalVirtPageToPhysFrame(asp, base_vpn + i, NULL);
        if (status != STATUS_PAGE_NOT_PRESENT) return 0;
    }

    return 1;
}

StStatus StVmm_InitGlobalDomain(
    enum StVmm_Domain domain __in, St_VirtPage base_vpn __in, St_VirtPage limit_vpn __in
)
{
    struct vmm_alloc_domain *alloc_domain;

    if (domain >= VMM_DOMAIN_MAX) return STATUS_INVALID_VALUE;
    if (limit_vpn < base_vpn) return STATUS_INVALID_VALUE;

    alloc_domain = &alloc_domain_list[domain];
    alloc_domain->head = NULL;
    alloc_domain->base_vpn = base_vpn;
    alloc_domain->limit_vpn = limit_vpn;
    alloc_domain->free_count = (St_PageCount)(limit_vpn - base_vpn + 1);
    alloc_domain->initialized = 1;

    return STATUS_SUCCESS;
}

StStatus StVmm_InitLocalDomain(
    StAddressSpace_StrongRef asp __in, St_VirtPage base_vpn __in, St_VirtPage limit_vpn __in
)
{
    if (!asp) return STATUS_INVALID_VALUE;
    if (limit_vpn < base_vpn) return STATUS_INVALID_VALUE;

    asp->user_alloc_head = NULL;
    asp->user_base_vpn = base_vpn;
    asp->user_limit_vpn = limit_vpn;
    asp->user_free_count = (St_PageCount)(limit_vpn - base_vpn + 1);

    return STATUS_SUCCESS;
}

void StVmm_RemoveLocalDomain(StAddressSpace_StrongRef asp __in)
{
    assert(asp);

    struct vmm_alloc_node *curr;
    struct vmm_alloc_node *next;
    uint32_t irq_state;

    irq_state = StA_SaveInterrupt();
    StA_DisableInterrupt();
    StThread_LockPreemption();

    curr = (struct vmm_alloc_node *)asp->user_alloc_head;
    while (curr) {
        next = curr->domain_next;
        remove_domain_node(get_local_head_slot(asp), curr);
        detach_owner_node(curr);
        release_alloc_node(curr);
        curr->asp = NULL;
        curr = next;
    }

    asp->user_alloc_head = NULL;
    asp->user_free_count = (St_PageCount)(asp->user_limit_vpn - asp->user_base_vpn + 1);

    StThread_UnlockPreemption();
    StA_RestoreInterrupt(irq_state);
}

StStatus StVmm_GetTotalGlobalPageCount(enum StVmm_Domain domain __in, St_PageCount *count __out)
{
    assert(count);

    struct vmm_alloc_domain *alloc_domain;

    if (domain >= VMM_DOMAIN_MAX) return STATUS_INVALID_VALUE;

    alloc_domain = &alloc_domain_list[domain];
    if (!alloc_domain->initialized) return STATUS_INVALID_VALUE;

    *count = (St_PageCount)(alloc_domain->limit_vpn - alloc_domain->base_vpn + 1);

    return STATUS_SUCCESS;
}

StStatus StVmm_GetFreeGlobalPageCount(enum StVmm_Domain domain __in, St_PageCount *count __out)
{
    assert(count);

    struct vmm_alloc_domain *alloc_domain;

    if (domain >= VMM_DOMAIN_MAX) return STATUS_INVALID_VALUE;

    alloc_domain = &alloc_domain_list[domain];
    if (!alloc_domain->initialized) return STATUS_INVALID_VALUE;

    *count = alloc_domain->free_count;

    return STATUS_SUCCESS;
}

StStatus StVmm_GetTotalLocalPageCount(StAddressSpace_StrongRef asp __in, St_PageCount *count __out)
{
    assert(count);

    if (!asp) return STATUS_INVALID_VALUE;

    *count = (St_PageCount)(asp->user_limit_vpn - asp->user_base_vpn + 1);

    return STATUS_SUCCESS;
}

StStatus StVmm_GetFreeLocalPageCount(StAddressSpace_StrongRef asp __in, St_PageCount *count __out)
{
    assert(count);

    if (!asp) return STATUS_INVALID_VALUE;

    *count = asp->user_free_count;

    return STATUS_SUCCESS;
}

StStatus StVmm_AllocateGlobalPage(
    enum StVmm_Domain domain __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in
)
{
    assert(vpn);

    StStatus status;
    struct vmm_alloc_node *new_node;
    struct vmm_alloc_domain *ad;
    St_VirtPage candidate_start;
    St_VirtPage search_start;
    uint32_t irq_state;

    if (domain >= VMM_DOMAIN_MAX) return STATUS_INVALID_VALUE;
    if (count == 0) return STATUS_INVALID_VALUE;

    ad = &alloc_domain_list[domain];
    if (!ad->initialized) return STATUS_CONFLICTING_STATE;

    irq_state = StA_SaveInterrupt();
    StA_DisableInterrupt();
    StThread_LockPreemption();

    if (ad->free_count < count) {
        status = STATUS_INSUFFICIENT_MEMORY;
        goto has_error;
    }
    if (!validate_domain_list(ad->head, "alloc-global")) {
        status = STATUS_SYSTEM_CORRUPTED;
        goto has_error;
    }

    search_start = ad->base_vpn;
    for (;;) {
        status = find_first_fit_from(
            ad->head,
            ad->base_vpn,
            ad->limit_vpn,
            count,
            search_start,
            &candidate_start
        );
        if (!CHECK_SUCCESS(status)) goto has_error;

        if (is_global_range_unmapped(candidate_start, count)) break;

        if (candidate_start >= ad->limit_vpn) {
            status = STATUS_INSUFFICIENT_MEMORY;
            goto has_error;
        }
        search_start = candidate_start + 1;
    }

    status = create_alloc_node(&new_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    new_node->base_vpn = candidate_start;
    new_node->limit_vpn = candidate_start + count;
    new_node->asp = NULL;
    new_node->domain = domain;
    init_alloc_node_metadata(new_node, alloc_flags, 0, VMM_BACKING_PHYSICAL);
    new_node->is_live = 1;

    status = attach_owner_node(new_node, owner);
    if (!CHECK_SUCCESS(status)) {
        release_alloc_node(new_node);
        goto has_error;
    }

    status = insert_domain_node_sorted(&ad->head, new_node);
    if (!CHECK_SUCCESS(status)) {
        detach_owner_node(new_node);
        release_alloc_node(new_node);
        goto has_error;
    }

    ad->free_count -= count;

    LOG_TRACE(LM_CAT_UNCLASSIFIED, "allocated %zu pages at %013zX\n", count, new_node->base_vpn);

    *vpn = new_node->base_vpn;

    StThread_UnlockPreemption();
    StA_RestoreInterrupt(irq_state);

    return STATUS_SUCCESS;

has_error:
    StThread_UnlockPreemption();
    StA_RestoreInterrupt(irq_state);
    return status;
}

StStatus StVmm_AllocateLocalPage(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
)
{
    assert(vpn);

    StStatus status;
    struct vmm_alloc_node *new_node;
    StAllocationOwner_StrongRef owner;
    struct vmm_alloc_node **head_slot;
    St_VirtPage candidate_start;
    St_VirtPage search_start;
    uint32_t irq_state;

    if (!asp) return STATUS_INVALID_VALUE;
    if (count == 0) return STATUS_INVALID_VALUE;

    owner = asp->process->alloc_owner;
    head_slot = get_local_head_slot(asp);

    irq_state = StA_SaveInterrupt();
    StA_DisableInterrupt();
    StThread_LockPreemption();

    if (asp->user_free_count < count) {
        status = STATUS_INSUFFICIENT_MEMORY;
        goto has_error;
    }
    if (!validate_domain_list(*head_slot, "alloc-local")) {
        status = STATUS_SYSTEM_CORRUPTED;
        goto has_error;
    }

    search_start = asp->user_base_vpn;
    for (;;) {
        status = find_first_fit_from(
            *head_slot,
            asp->user_base_vpn,
            asp->user_limit_vpn,
            count,
            search_start,
            &candidate_start
        );
        if (!CHECK_SUCCESS(status)) goto has_error;

        if (is_local_range_unmapped(asp, candidate_start, count)) break;

        if (candidate_start >= asp->user_limit_vpn) {
            status = STATUS_INSUFFICIENT_MEMORY;
            goto has_error;
        }
        search_start = candidate_start + 1;
    }

    status = create_alloc_node(&new_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    new_node->base_vpn = candidate_start;
    new_node->limit_vpn = candidate_start + count;
    new_node->asp = (StAddressSpace_InternalRef)asp;
    new_node->domain = VMM_DOMAIN_MAX;
    init_alloc_node_metadata(
        new_node,
        alloc_flags,
        map_flags,
        local_backing_type_from_map_flags(map_flags)
    );
    new_node->is_live = 1;

    status = attach_owner_node(new_node, owner);
    if (!CHECK_SUCCESS(status)) {
        release_alloc_node(new_node);
        goto has_error;
    }

    status = insert_domain_node_sorted(head_slot, new_node);
    if (!CHECK_SUCCESS(status)) {
        detach_owner_node(new_node);
        release_alloc_node(new_node);
        goto has_error;
    }

    asp->user_free_count -= count;

    *vpn = new_node->base_vpn;

    StThread_UnlockPreemption();
    StA_RestoreInterrupt(irq_state);

    return STATUS_SUCCESS;

has_error:
    StThread_UnlockPreemption();
    StA_RestoreInterrupt(irq_state);
    return status;
}

StStatus StVmm_AllocateGlobalPageTo(
    enum StVmm_Domain domain __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in
)
{
    StStatus status;
    struct vmm_alloc_node *new_node;
    struct vmm_alloc_domain *ad;
    St_VirtPage limit_vpn;
    uint32_t irq_state;

    if (domain >= VMM_DOMAIN_MAX) return STATUS_INVALID_VALUE;
    if (count == 0) return STATUS_INVALID_VALUE;

    status = make_limit_exclusive(vpn, count, &limit_vpn);
    if (!CHECK_SUCCESS(status)) return status;

    ad = &alloc_domain_list[domain];
    if (!ad->initialized) return STATUS_CONFLICTING_STATE;

    if (vpn < ad->base_vpn || limit_vpn - 1 > ad->limit_vpn) return STATUS_INVALID_VALUE;

    irq_state = StA_SaveInterrupt();
    StA_DisableInterrupt();
    StThread_LockPreemption();

    if (ad->free_count < count) {
        status = STATUS_INSUFFICIENT_MEMORY;
        goto has_error;
    }
    if (!validate_domain_list(ad->head, "alloc-global-to")) {
        status = STATUS_SYSTEM_CORRUPTED;
        goto has_error;
    }

    if (find_overlap(ad->head, vpn, count)) {
        status = STATUS_CONFLICTING_STATE;
        goto has_error;
    }
    if (!is_global_range_unmapped(vpn, count)) {
        status = STATUS_CONFLICTING_STATE;
        goto has_error;
    }

    status = create_alloc_node(&new_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    new_node->base_vpn = vpn;
    new_node->limit_vpn = limit_vpn;
    new_node->asp = NULL;
    new_node->domain = domain;
    init_alloc_node_metadata(new_node, alloc_flags, 0, VMM_BACKING_PHYSICAL);
    new_node->is_live = 1;

    status = attach_owner_node(new_node, owner);
    if (!CHECK_SUCCESS(status)) {
        release_alloc_node(new_node);
        goto has_error;
    }

    status = insert_domain_node_sorted(&ad->head, new_node);
    if (!CHECK_SUCCESS(status)) {
        detach_owner_node(new_node);
        release_alloc_node(new_node);
        goto has_error;
    }

    ad->free_count -= count;

    LOG_TRACE(
        LM_CAT_UNCLASSIFIED,
        "allocated %zu pages at %013zX (fixed)\n",
        count,
        new_node->base_vpn
    );

    StThread_UnlockPreemption();
    StA_RestoreInterrupt(irq_state);

    return STATUS_SUCCESS;

has_error:
    StThread_UnlockPreemption();
    StA_RestoreInterrupt(irq_state);
    return status;
}

StStatus StVmm_AllocateLocalPageTo(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
)
{
    StStatus status;
    struct vmm_alloc_node *new_node;
    StAllocationOwner_StrongRef owner;
    struct vmm_alloc_node **head_slot;
    St_VirtPage limit_vpn;
    uint32_t irq_state;

    if (!asp) return STATUS_INVALID_VALUE;
    if (count == 0) return STATUS_INVALID_VALUE;

    status = make_limit_exclusive(vpn, count, &limit_vpn);
    if (!CHECK_SUCCESS(status)) return status;

    if (vpn < asp->user_base_vpn || limit_vpn - 1 > asp->user_limit_vpn)
        return STATUS_INVALID_VALUE;

    owner = asp->process->alloc_owner;
    head_slot = get_local_head_slot(asp);

    irq_state = StA_SaveInterrupt();
    StA_DisableInterrupt();
    StThread_LockPreemption();

    if (asp->user_free_count < count) {
        status = STATUS_INSUFFICIENT_MEMORY;
        goto has_error;
    }
    if (!validate_domain_list(*head_slot, "alloc-local-to")) {
        status = STATUS_SYSTEM_CORRUPTED;
        goto has_error;
    }

    if (find_overlap(*head_slot, vpn, count)) {
        status = STATUS_CONFLICTING_STATE;
        goto has_error;
    }
    if (!is_local_range_unmapped(asp, vpn, count)) {
        status = STATUS_CONFLICTING_STATE;
        goto has_error;
    }

    status = create_alloc_node(&new_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    new_node->base_vpn = vpn;
    new_node->limit_vpn = limit_vpn;
    new_node->asp = (StAddressSpace_InternalRef)asp;
    new_node->domain = VMM_DOMAIN_MAX;
    init_alloc_node_metadata(
        new_node,
        alloc_flags,
        map_flags,
        local_backing_type_from_map_flags(map_flags)
    );
    new_node->is_live = 1;

    status = attach_owner_node(new_node, owner);
    if (!CHECK_SUCCESS(status)) {
        release_alloc_node(new_node);
        goto has_error;
    }

    status = insert_domain_node_sorted(head_slot, new_node);
    if (!CHECK_SUCCESS(status)) {
        detach_owner_node(new_node);
        release_alloc_node(new_node);
        goto has_error;
    }

    asp->user_free_count -= count;

    StThread_UnlockPreemption();
    StA_RestoreInterrupt(irq_state);

    return STATUS_SUCCESS;

has_error:
    StThread_UnlockPreemption();
    StA_RestoreInterrupt(irq_state);
    return status;
}

void StVmm_FreeGlobalPage(
    enum StVmm_Domain domain __in, St_VirtPage vpn __in, St_PageCount count __in
)
{
    struct vmm_alloc_domain *ad;
    struct vmm_alloc_node *node;
    uint32_t irq_state;

    if (domain >= VMM_DOMAIN_MAX) return;
    if (count == 0) return;

    ad = &alloc_domain_list[domain];
    if (!ad->initialized) return;

    irq_state = StA_SaveInterrupt();
    StA_DisableInterrupt();
    StThread_LockPreemption();

    if (!validate_domain_list(ad->head, "free-global")) {
        StThread_UnlockPreemption();
        StA_RestoreInterrupt(irq_state);
        return;
    }

    node = find_exact(ad->head, vpn, count);

    if (node) {
        remove_domain_node(&ad->head, node);
        detach_owner_node(node);
        release_alloc_node(node);

        ad->free_count += count;

        LOG_TRACE(LM_CAT_UNCLASSIFIED, "freed %zu pages at %013zX\n", count, vpn);
    } else {
        LOG_ERROR(LM_CAT_UNCLASSIFIED, "vmm node not found for vpn %013zX\n", vpn);
    }

    StThread_UnlockPreemption();
    StA_RestoreInterrupt(irq_state);
}

void StVmm_FreeLocalPage(
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, St_PageCount count __in
)
{
    StStatus status;
    St_VirtPage limit_vpn;
    struct vmm_alloc_node **head_slot;
    struct vmm_alloc_node *node;
    uint32_t irq_state;

    if (!asp) return;
    if (count == 0) return;

    status = make_limit_exclusive(vpn, count, &limit_vpn);
    if (!CHECK_SUCCESS(status)) return;
    if (vpn < asp->user_base_vpn || limit_vpn - 1 > asp->user_limit_vpn) return;

    head_slot = get_local_head_slot(asp);

    irq_state = StA_SaveInterrupt();
    StA_DisableInterrupt();
    StThread_LockPreemption();

    if (!validate_domain_list(*head_slot, "free-local")) {
        StThread_UnlockPreemption();
        StA_RestoreInterrupt(irq_state);
        return;
    }

    node = find_exact(*head_slot, vpn, count);
    if (node) {
        remove_domain_node(head_slot, node);
        detach_owner_node(node);
        release_alloc_node(node);

        asp->user_free_count += count;

        LOG_TRACE(LM_CAT_UNCLASSIFIED, "freed %zu pages at %013zX (local)\n", count, vpn);
    } else {
        LOG_ERROR(LM_CAT_UNCLASSIFIED, "local vmm node not found for vpn %013zX\n", vpn);
    }

    StThread_UnlockPreemption();
    StA_RestoreInterrupt(irq_state);
}

StStatus StVmm_GetGlobalAllocationRange(
    enum StVmm_Domain domain __in,
    St_VirtPage vpn __in,
    St_VirtPage *begin_vpn __out_optional,
    St_VirtPage *end_vpn __out_optional
)
{
    struct vmm_alloc_domain *ad;
    struct vmm_alloc_node *node;
    uint32_t irq_state;
    StStatus status = STATUS_SUCCESS;

    if (domain >= VMM_DOMAIN_MAX) return STATUS_INVALID_VALUE;

    ad = &alloc_domain_list[domain];
    if (!ad->initialized) return STATUS_CONFLICTING_STATE;
    if (vpn < ad->base_vpn || vpn > ad->limit_vpn) return STATUS_INVALID_VALUE;

    irq_state = StA_SaveInterrupt();
    StA_DisableInterrupt();
    StThread_LockPreemption();

    if (!validate_domain_list(ad->head, "get-global-range")) {
        status = STATUS_SYSTEM_CORRUPTED;
        goto done;
    }

    node = find_overlap(ad->head, vpn, 1);
    if (!node) {
        status = STATUS_NOT_ALLOCATED;
        goto done;
    }

    if (begin_vpn) *begin_vpn = node->base_vpn;
    if (end_vpn) *end_vpn = node->limit_vpn;

done:
    StThread_UnlockPreemption();
    StA_RestoreInterrupt(irq_state);
    return status;
}

StStatus StVmm_GetLocalAllocationRange(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_VirtPage *begin_vpn __out_optional,
    St_VirtPage *end_vpn __out_optional
)
{
    struct vmm_alloc_node **head_slot;
    struct vmm_alloc_node *node;
    uint32_t irq_state;
    StStatus status = STATUS_SUCCESS;

    if (!asp) return STATUS_INVALID_VALUE;
    if (vpn < asp->user_base_vpn || vpn > asp->user_limit_vpn) return STATUS_INVALID_VALUE;

    head_slot = get_local_head_slot(asp);

    irq_state = StA_SaveInterrupt();
    StA_DisableInterrupt();
    StThread_LockPreemption();

    if (!validate_domain_list(*head_slot, "get-local-range")) {
        status = STATUS_SYSTEM_CORRUPTED;
        goto done;
    }

    node = find_overlap(*head_slot, vpn, 1);
    if (!node) {
        status = STATUS_NOT_ALLOCATED;
        goto done;
    }

    if (begin_vpn) *begin_vpn = node->base_vpn;
    if (end_vpn) *end_vpn = node->limit_vpn;

done:
    StThread_UnlockPreemption();
    StA_RestoreInterrupt(irq_state);
    return status;
}

StStatus StVmm_GetLocalPageInfo(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    struct StVmm_PageInfo *info __out
)
{
    assert(info);

    struct vmm_alloc_node **head_slot;
    struct vmm_alloc_node *node;
    uint32_t irq_state;
    StStatus status = STATUS_SUCCESS;

    if (!asp) return STATUS_INVALID_VALUE;
    if (vpn < asp->user_base_vpn || vpn > asp->user_limit_vpn) return STATUS_INVALID_VALUE;

    head_slot = get_local_head_slot(asp);

    irq_state = StA_SaveInterrupt();
    StA_DisableInterrupt();
    StThread_LockPreemption();

    if (!validate_domain_list(*head_slot, "get-local-page-info")) {
        status = STATUS_SYSTEM_CORRUPTED;
        goto done;
    }

    node = find_overlap(*head_slot, vpn, 1);
    if (!node) {
        status = STATUS_NOT_ALLOCATED;
        goto done;
    }

    info->backing_type = node->backing_type;
    info->alloc_flags = node->alloc_flags;
    info->map_flags = node->map_flags;
    info->image_backing = node->image_backing;

done:
    StThread_UnlockPreemption();
    StA_RestoreInterrupt(irq_state);
    return status;
}

StStatus StVmm_SetLocalPageImageBacking(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    const struct StMm_ImageBacking *backing __in
)
{
    assert(backing);

    StStatus status;
    St_VirtPage limit_vpn;
    struct vmm_alloc_node **head_slot;
    struct vmm_alloc_node *node;
    uint32_t irq_state;

    if (!asp) return STATUS_INVALID_VALUE;
    if (count == 0) return STATUS_INVALID_VALUE;

    status = make_limit_exclusive(vpn, count, &limit_vpn);
    if (!CHECK_SUCCESS(status)) return status;
    if (vpn < asp->user_base_vpn || limit_vpn - 1 > asp->user_limit_vpn) {
        return STATUS_INVALID_VALUE;
    }

    head_slot = get_local_head_slot(asp);

    irq_state = StA_SaveInterrupt();
    StA_DisableInterrupt();
    StThread_LockPreemption();

    if (!validate_domain_list(*head_slot, "set-local-image-backing")) {
        status = STATUS_SYSTEM_CORRUPTED;
        goto done;
    }

    node = find_exact(*head_slot, vpn, count);
    if (!node) {
        status = STATUS_NOT_ALLOCATED;
        goto done;
    }

    if (node->backing_type != VMM_BACKING_DEMAND_ZERO) {
        status = STATUS_CONFLICTING_STATE;
        goto done;
    }

    node->backing_type = VMM_BACKING_IMAGE;
    node->image_backing = *backing;
    status = STATUS_SUCCESS;

done:
    StThread_UnlockPreemption();
    StA_RestoreInterrupt(irq_state);
    return status;
}
