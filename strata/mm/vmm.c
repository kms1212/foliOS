#include <strata/mm/vmm.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <strata/thread.h>

#include <strata/compiler.h>
#include <strata/log.h>
#include <strata/mm/asp.h>
#include <strata/mm/owner.h>
#include <strata/mm/types.h>
#include <strata/status.h>
#include <strata/types.h>

#include "internal.h"

#define MODULE_NAME "vmm"

#define EARLY_ALLOC_NODE_POOL_COUNT 1024

static struct vmm_alloc_domain alloc_domain_list[VMM_DOMAIN_MAX] = {
    [VMM_DOMAIN_MODULE] = {.initialized = 0},
    [VMM_DOMAIN_KERNEL_FAST] = {.initialized = 0},
    [VMM_DOMAIN_KERNEL_SLOW] = {.initialized = 0},
    [VMM_DOMAIN_IO] = {.initialized = 0},
    [VMM_DOMAIN_KRT_GLOBAL] = {.initialized = 0},
};

static struct vmm_alloc_node early_alloc_node_pool[EARLY_ALLOC_NODE_POOL_COUNT];
static int early_alloc_node_pool_used_count = 0;

static inline struct vmm_alloc_node **get_local_head_slot(struct StMm_AddressSpace *asp)
{
    return (struct vmm_alloc_node **)&asp->user_alloc_head;
}

static StStatus make_limit_exclusive(
    St_VirtPage base_vpn __in, St_PageCount count __in, St_VirtPage *limit_out __out
)
{
    St_VirtPage limit;

    if (count == 0) return STATUS_INVALID_VALUE;

    limit = base_vpn + (St_VirtPage)count;
    if (limit < base_vpn) return STATUS_INVALID_VALUE;

    if (limit_out) *limit_out = limit;

    return STATUS_SUCCESS;
}

static StStatus make_domain_limit_exclusive(St_VirtPage limit_inclusive __in, St_VirtPage *limit_out __out)
{
    if (limit_inclusive == (St_VirtPage)-1) return STATUS_CONFLICTING_STATE;

    if (limit_out) *limit_out = limit_inclusive + 1;

    return STATUS_SUCCESS;
}

static StStatus create_alloc_node(struct vmm_alloc_node **node)
{
    int i;

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

    // TODO: allocate a new node from slab allocator.
    return STATUS_NOT_IMPLEMENTED;
}

static void attach_owner_node(struct vmm_alloc_node *node, struct StMm_AllocationOwner *owner)
{
    struct vmm_alloc_node *owner_last;

    node->owner = owner;
    node->owner_prev = NULL;
    node->owner_next = NULL;

    if (!owner) return;

    owner_last = (struct vmm_alloc_node *)owner->last_vmm_node;
    node->owner_prev = owner_last;
    if (owner_last) {
        owner_last->owner_next = node;
    } else {
        owner->first_vmm_node = node;
    }
    owner->last_vmm_node = node;
}

static void detach_owner_node(struct vmm_alloc_node *node)
{
    struct StMm_AllocationOwner *owner;

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
}

static StStatus insert_domain_node_sorted(
    struct vmm_alloc_node **head_slot __inout, struct vmm_alloc_node *node __in
)
{
    struct vmm_alloc_node *prev = NULL;
    struct vmm_alloc_node *curr = *head_slot;

    while (curr && curr->base_vpn < node->base_vpn) {
        prev = curr;
        curr = curr->domain_next;
    }

    if (curr && curr->base_vpn == node->base_vpn) return STATUS_DUPLICATE_ENTRY;

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

static void remove_domain_node(struct vmm_alloc_node **head_slot __inout, struct vmm_alloc_node *node __in)
{
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

static StStatus find_first_fit(
    struct vmm_alloc_node *head __in,
    St_VirtPage domain_base_vpn __in,
    St_VirtPage domain_limit_vpn __in,
    St_PageCount count __in,
    St_VirtPage *result_vpn __out
)
{
    struct vmm_alloc_node *curr = head;
    St_VirtPage candidate_start = domain_base_vpn;
    St_VirtPage domain_limit_exclusive;
    StStatus status;

    if (count == 0) return STATUS_INVALID_VALUE;

    status = make_domain_limit_exclusive(domain_limit_vpn, &domain_limit_exclusive);
    if (!CHECK_SUCCESS(status)) return status;

    while (curr) {
        if (curr->base_vpn > candidate_start) {
            St_PageCount gap = curr->base_vpn - candidate_start;
            if (gap >= count) {
                if (result_vpn) *result_vpn = candidate_start;
                return STATUS_SUCCESS;
            }
        }

        if (curr->limit_vpn > candidate_start) candidate_start = curr->limit_vpn;
        if (candidate_start >= domain_limit_exclusive) return STATUS_INSUFFICIENT_MEMORY;

        curr = curr->domain_next;
    }

    if (domain_limit_exclusive > candidate_start) {
        St_PageCount gap = domain_limit_exclusive - candidate_start;
        if (gap >= count) {
            if (result_vpn) *result_vpn = candidate_start;
            return STATUS_SUCCESS;
        }
    }

    return STATUS_INSUFFICIENT_MEMORY;
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
    alloc_domain->free_count = limit_vpn - base_vpn + 1;
    alloc_domain->initialized = 1;

    return STATUS_SUCCESS;
}

StStatus StVmm_InitLocalDomain(
    struct StMm_AddressSpace *asp __in, St_VirtPage base_vpn __in, St_VirtPage limit_vpn __in
)
{
    if (!asp) return STATUS_INVALID_VALUE;
    if (limit_vpn < base_vpn) return STATUS_INVALID_VALUE;

    asp->user_alloc_head = NULL;
    asp->user_base_vpn = base_vpn;
    asp->user_limit_vpn = limit_vpn;
    asp->user_free_count = limit_vpn - base_vpn + 1;

    return STATUS_SUCCESS;
}

StStatus StVmm_RemoveLocalDomain(struct StMm_AddressSpace *asp __in)
{
    struct vmm_alloc_node *curr;
    struct vmm_alloc_node *next;

    if (!asp) return STATUS_INVALID_VALUE;

    curr = (struct vmm_alloc_node *)asp->user_alloc_head;
    while (curr) {
        next = curr->domain_next;
        remove_domain_node(get_local_head_slot(asp), curr);
        detach_owner_node(curr);
        curr->is_live = 0;
        curr->asp = NULL;
        curr = next;
    }

    asp->user_alloc_head = NULL;
    asp->user_free_count = asp->user_limit_vpn - asp->user_base_vpn + 1;

    return STATUS_SUCCESS;
}

StStatus StVmm_GetTotalGlobalPageCount(enum StVmm_Domain domain __in, St_PageCount *count __out)
{
    struct vmm_alloc_domain *alloc_domain;

    if (domain >= VMM_DOMAIN_MAX) return STATUS_INVALID_VALUE;
    if (!count) return STATUS_INVALID_VALUE;

    alloc_domain = &alloc_domain_list[domain];
    if (!alloc_domain->initialized) return STATUS_INVALID_VALUE;

    *count = alloc_domain->limit_vpn - alloc_domain->base_vpn + 1;

    return STATUS_SUCCESS;
}

StStatus StVmm_GetFreeGlobalPageCount(enum StVmm_Domain domain, St_PageCount *count __out)
{
    struct vmm_alloc_domain *alloc_domain;

    if (domain >= VMM_DOMAIN_MAX) return STATUS_INVALID_VALUE;
    if (!count) return STATUS_INVALID_VALUE;

    alloc_domain = &alloc_domain_list[domain];
    if (!alloc_domain->initialized) return STATUS_INVALID_VALUE;

    *count = alloc_domain->free_count;

    return STATUS_SUCCESS;
}

StStatus StVmm_GetTotalLocalPageCount(
    struct StMm_AddressSpace *asp __in, St_PageCount *count __out
)
{
    if (!asp || !count) return STATUS_INVALID_VALUE;

    *count = asp->user_limit_vpn - asp->user_base_vpn + 1;

    return STATUS_SUCCESS;
}

StStatus StVmm_GetFreeLocalPageCount(struct StMm_AddressSpace *asp __in, St_PageCount *count __out)
{
    if (!asp || !count) return STATUS_INVALID_VALUE;

    *count = asp->user_free_count;

    return STATUS_SUCCESS;
}

StStatus StVmm_AllocateGlobalPage(
    enum StVmm_Domain domain __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    struct StMm_AllocationOwner *owner __in,
    StMm_AllocFlags alloc_flags __in
)
{
    StStatus status;
    struct vmm_alloc_node *new_node;
    struct vmm_alloc_domain *ad;
    St_VirtPage candidate_start;

    if (!vpn) return STATUS_INVALID_VALUE;
    if (domain >= VMM_DOMAIN_MAX) return STATUS_INVALID_VALUE;
    if (count == 0) return STATUS_INVALID_VALUE;

    ad = &alloc_domain_list[domain];
    if (!ad->initialized) return STATUS_CONFLICTING_STATE;

    StThread_LockPreemption();

    if (ad->free_count < count) {
        status = STATUS_INSUFFICIENT_MEMORY;
        goto has_error;
    }

    status = find_first_fit(ad->head, ad->base_vpn, ad->limit_vpn, count, &candidate_start);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = create_alloc_node(&new_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    new_node->base_vpn = candidate_start;
    new_node->limit_vpn = candidate_start + count;
    new_node->asp = NULL;
    new_node->domain = domain;
    new_node->alloc_type = (alloc_flags & AF_VMM_HIDDEN_AT_MAP) ? AT_MAP : AT_ALLOC;
    new_node->is_live = 1;

    attach_owner_node(new_node, owner);

    status = insert_domain_node_sorted(&ad->head, new_node);
    if (!CHECK_SUCCESS(status)) {
        detach_owner_node(new_node);
        new_node->is_live = 0;
        goto has_error;
    }

    ad->free_count -= count;

    LOG_TRACE(LM_CAT_UNCLASSIFIED, "allocated %zu pages at %013zX\n", count, new_node->base_vpn);

    *vpn = new_node->base_vpn;

    StThread_UnlockPreemption();

    return STATUS_SUCCESS;

has_error:
    StThread_UnlockPreemption();
    return status;
}

StStatus StVmm_AllocateLocalPage(
    struct StMm_AddressSpace *asp __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in
)
{
    StStatus status;
    struct vmm_alloc_node *new_node;
    struct StMm_AllocationOwner *owner;
    struct vmm_alloc_node **head_slot;
    St_VirtPage candidate_start;

    if (!asp || !vpn) return STATUS_INVALID_VALUE;
    if (count == 0) return STATUS_INVALID_VALUE;

    owner = &asp->process->alloc_owner;
    head_slot = get_local_head_slot(asp);

    StThread_LockPreemption();

    if (asp->user_free_count < count) {
        status = STATUS_INSUFFICIENT_MEMORY;
        goto has_error;
    }

    status =
        find_first_fit(*head_slot, asp->user_base_vpn, asp->user_limit_vpn, count, &candidate_start);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = create_alloc_node(&new_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    new_node->base_vpn = candidate_start;
    new_node->limit_vpn = candidate_start + count;
    new_node->asp = asp;
    new_node->domain = VMM_DOMAIN_MAX;
    new_node->alloc_type = (alloc_flags & AF_VMM_HIDDEN_AT_MAP) ? AT_MAP : AT_ALLOC;
    new_node->is_live = 1;

    attach_owner_node(new_node, owner);

    status = insert_domain_node_sorted(head_slot, new_node);
    if (!CHECK_SUCCESS(status)) {
        detach_owner_node(new_node);
        new_node->is_live = 0;
        goto has_error;
    }

    asp->user_free_count -= count;

    *vpn = new_node->base_vpn;

    StThread_UnlockPreemption();

    return STATUS_SUCCESS;

has_error:
    StThread_UnlockPreemption();
    return status;
}

StStatus StVmm_AllocateGlobalPageTo(
    enum StVmm_Domain domain __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    struct StMm_AllocationOwner *owner __in,
    StMm_AllocFlags alloc_flags __in
)
{
    StStatus status;
    struct vmm_alloc_node *new_node;
    struct vmm_alloc_domain *ad;
    St_VirtPage limit_vpn;

    if (domain >= VMM_DOMAIN_MAX) return STATUS_INVALID_VALUE;
    if (count == 0) return STATUS_INVALID_VALUE;

    status = make_limit_exclusive(vpn, count, &limit_vpn);
    if (!CHECK_SUCCESS(status)) return status;

    ad = &alloc_domain_list[domain];
    if (!ad->initialized) return STATUS_CONFLICTING_STATE;

    if (vpn < ad->base_vpn || limit_vpn - 1 > ad->limit_vpn) return STATUS_INVALID_VALUE;

    StThread_LockPreemption();

    if (ad->free_count < count) {
        status = STATUS_INSUFFICIENT_MEMORY;
        goto has_error;
    }

    if (find_overlap(ad->head, vpn, count)) {
        status = STATUS_CONFLICTING_STATE;
        goto has_error;
    }

    status = create_alloc_node(&new_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    new_node->base_vpn = vpn;
    new_node->limit_vpn = limit_vpn;
    new_node->asp = NULL;
    new_node->domain = domain;
    new_node->alloc_type = (alloc_flags & AF_VMM_HIDDEN_AT_MAP) ? AT_MAP : AT_ALLOC;
    new_node->is_live = 1;

    attach_owner_node(new_node, owner);

    status = insert_domain_node_sorted(&ad->head, new_node);
    if (!CHECK_SUCCESS(status)) {
        detach_owner_node(new_node);
        new_node->is_live = 0;
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

    return STATUS_SUCCESS;

has_error:
    StThread_UnlockPreemption();
    return status;
}

StStatus StVmm_AllocateLocalPageTo(
    struct StMm_AddressSpace *asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in
)
{
    StStatus status;
    struct vmm_alloc_node *new_node;
    struct StMm_AllocationOwner *owner;
    struct vmm_alloc_node **head_slot;
    St_VirtPage limit_vpn;

    if (!asp) return STATUS_INVALID_VALUE;
    if (count == 0) return STATUS_INVALID_VALUE;

    status = make_limit_exclusive(vpn, count, &limit_vpn);
    if (!CHECK_SUCCESS(status)) return status;

    if (vpn < asp->user_base_vpn || limit_vpn - 1 > asp->user_limit_vpn) return STATUS_INVALID_VALUE;

    owner = &asp->process->alloc_owner;
    head_slot = get_local_head_slot(asp);

    StThread_LockPreemption();

    if (asp->user_free_count < count) {
        status = STATUS_INSUFFICIENT_MEMORY;
        goto has_error;
    }

    if (find_overlap(*head_slot, vpn, count)) {
        status = STATUS_CONFLICTING_STATE;
        goto has_error;
    }

    status = create_alloc_node(&new_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    new_node->base_vpn = vpn;
    new_node->limit_vpn = limit_vpn;
    new_node->asp = asp;
    new_node->domain = VMM_DOMAIN_MAX;
    new_node->alloc_type = (alloc_flags & AF_VMM_HIDDEN_AT_MAP) ? AT_MAP : AT_ALLOC;
    new_node->is_live = 1;

    attach_owner_node(new_node, owner);

    status = insert_domain_node_sorted(head_slot, new_node);
    if (!CHECK_SUCCESS(status)) {
        detach_owner_node(new_node);
        new_node->is_live = 0;
        goto has_error;
    }

    asp->user_free_count -= count;

    StThread_UnlockPreemption();

    return STATUS_SUCCESS;

has_error:
    StThread_UnlockPreemption();
    return status;
}

void StVmm_FreeGlobalPage(enum StVmm_Domain domain __in, St_VirtPage vpn __in, St_PageCount count __in)
{
    struct vmm_alloc_domain *ad;
    struct vmm_alloc_node *node;

    if (domain >= VMM_DOMAIN_MAX) return;
    if (count == 0) return;

    ad = &alloc_domain_list[domain];
    if (!ad->initialized) return;

    StThread_LockPreemption();

    node = find_exact(ad->head, vpn, count);

    if (node) {
        remove_domain_node(&ad->head, node);
        detach_owner_node(node);
        node->is_live = 0;

        ad->free_count += count;

        LOG_TRACE(LM_CAT_UNCLASSIFIED, "freed %zu pages at %013zX\n", count, vpn);
    } else {
        LOG_ERROR(LM_CAT_UNCLASSIFIED, "vmm node not found for vpn %013zX\n", vpn);
    }

    StThread_UnlockPreemption();
}

void StVmm_FreeLocalPage(
    struct StMm_AddressSpace *asp __in, St_VirtPage vpn __in, St_PageCount count __in
)
{
    StStatus status;
    St_VirtPage limit_vpn;
    struct vmm_alloc_node **head_slot;
    struct vmm_alloc_node *node;

    if (!asp) return;
    if (count == 0) return;

    status = make_limit_exclusive(vpn, count, &limit_vpn);
    if (!CHECK_SUCCESS(status)) return;
    if (vpn < asp->user_base_vpn || limit_vpn - 1 > asp->user_limit_vpn) return;

    head_slot = get_local_head_slot(asp);

    StThread_LockPreemption();

    node = find_exact(*head_slot, vpn, count);
    if (node) {
        remove_domain_node(head_slot, node);
        detach_owner_node(node);
        node->is_live = 0;

        asp->user_free_count += count;

        LOG_TRACE(LM_CAT_UNCLASSIFIED, "freed %zu pages at %013zX (local)\n", count, vpn);
    } else {
        LOG_ERROR(LM_CAT_UNCLASSIFIED, "local vmm node not found for vpn %013zX\n", vpn);
    }

    StThread_UnlockPreemption();
}
