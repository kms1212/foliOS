#include <strata/mm/vmm.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <strata/thread.h>
#include <string.h>

#include <strata/rb.h>

#include <strata/arch/cpufeatures.h>
#include <strata/arch/intrinsics/invlpg.h>
#include <strata/arch/intrinsics/register.h>
#include <strata/arch/mmu.h>

#include <strata/log.h>
#include <strata/macros.h>
#include <strata/panic.h>
#include <strata/status.h>

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

static StStatus create_alloc_node(struct vmm_alloc_node **node)
{
    if (early_alloc_node_pool_used_count < EARLY_ALLOC_NODE_POOL_COUNT) {
        if (node) *node = &early_alloc_node_pool[early_alloc_node_pool_used_count++];
        return STATUS_SUCCESS;
    } else {
        // TODO: allocate a new node from slab allocator.
        return STATUS_UNIMPLEMENTED;
    }
}

static int compare_alloc(struct StRbtree_Node *node1, struct StRbtree_Node *node2)
{
    struct vmm_alloc_node *n1 = rb_entry(node1, struct vmm_alloc_node, rbnode);
    struct vmm_alloc_node *n2 = rb_entry(node2, struct vmm_alloc_node, rbnode);

    if (n1->base_vpn < n2->base_vpn) return -1;
    if (n1->base_vpn > n2->base_vpn) return 1;
    return 0;
}

static struct vmm_alloc_node *find_overlap(
    struct StRbtree *tree, St_VirtPage base_vpn, St_PageCount count
)
{
    struct StRbtree_Node *node = tree->root.left;
    St_VirtPage limit_vpn = base_vpn + count;

    while (node != &tree->nil) {
        struct vmm_alloc_node *vmm_node = rb_entry(node, struct vmm_alloc_node, rbnode);

        // Check for overlap: [base_vpn, limit_vpn) vs [vmm_node->base_vpn, vmm_node->limit_vpn)
        if (base_vpn < vmm_node->limit_vpn && vmm_node->base_vpn < limit_vpn) {
            return vmm_node;
        }

        if (base_vpn < vmm_node->base_vpn) {
            node = node->left;
        } else {
            node = node->right;
        }
    }

    return NULL;
}

StStatus StVmm_InitGlobalDomain(
    enum StVmm_Domain domain __in, St_VirtPage base_vpn __in, St_VirtPage limit_vpn __in
)
{
    struct vmm_alloc_domain *alloc_domain;

    if (domain >= VMM_DOMAIN_MAX) return STATUS_INVALID_VALUE;

    alloc_domain = &alloc_domain_list[domain];

    StRbtree_Create(&alloc_domain->rbtree, compare_alloc);
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
    StRbtree_Create(&asp->user_rbtree, compare_alloc);
    asp->user_base_vpn = base_vpn;
    asp->user_limit_vpn = limit_vpn;
    asp->user_free_count = limit_vpn - base_vpn + 1;

    return STATUS_SUCCESS;
}

StStatus StVmm_RemoveLocalDomain(struct StMm_AddressSpace *asp __in)
{
    StRbtree_Destroy(&asp->user_rbtree);
    return STATUS_SUCCESS;
}

StStatus StVmm_GetTotalGlobalPageCount(enum StVmm_Domain domain __in, St_PageCount *count __out)
{
    struct vmm_alloc_domain *alloc_domain;

    if (domain >= VMM_DOMAIN_MAX) return STATUS_INVALID_VALUE;

    alloc_domain = &alloc_domain_list[domain];

    if (!alloc_domain->initialized) return STATUS_INVALID_VALUE;

    *count = alloc_domain->limit_vpn - alloc_domain->base_vpn + 1;

    return STATUS_SUCCESS;
}

StStatus StVmm_GetFreeGlobalPageCount(enum StVmm_Domain domain, St_PageCount *count __out)
{
    struct vmm_alloc_domain *alloc_domain;

    if (domain >= VMM_DOMAIN_MAX) return STATUS_INVALID_VALUE;

    alloc_domain = &alloc_domain_list[domain];

    if (!alloc_domain->initialized) return STATUS_INVALID_VALUE;

    *count = alloc_domain->free_count;

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
    struct StRbtree_Node *curr_rb;
    St_VirtPage candidate_start;

    if (domain >= VMM_DOMAIN_MAX) return STATUS_INVALID_VALUE;
    if (count == 0) return STATUS_INVALID_VALUE;

    ad = &alloc_domain_list[domain];
    if (!ad->initialized) return STATUS_CONFLICTING_STATE;

    StThread_LockPreemption();

    curr_rb = StRbtree_Min(&ad->rbtree);
    candidate_start = ad->base_vpn;

    while (curr_rb != NULL) {
        struct vmm_alloc_node *curr_node = rb_entry(curr_rb, struct vmm_alloc_node, rbnode);

        if (curr_node->base_vpn > candidate_start) {
            size_t gap = curr_node->base_vpn - candidate_start;
            if (gap >= count) {
                goto found_hole;
            }
        }

        candidate_start = curr_node->limit_vpn;

        curr_rb = StRbtree_Successor(&ad->rbtree, curr_rb);
    }

    if (ad->limit_vpn > candidate_start) {
        size_t gap = ad->limit_vpn - candidate_start;
        if (gap >= count) {
            goto found_hole;
        }
    }

    StThread_UnlockPreemption();

    return STATUS_INSUFFICIENT_MEMORY;

found_hole:
    status = create_alloc_node(&new_node);
    if (!CHECK_SUCCESS(status)) {
        return status;
    }

    new_node->base_vpn = candidate_start;
    new_node->limit_vpn = candidate_start + count;
    new_node->owner = owner;
    new_node->asp = NULL;
    new_node->domain = domain;

    if (owner) {
        new_node->owner_prev = owner->last_vmm_node;
        new_node->owner_next = NULL;

        if (owner->last_vmm_node) {
            ((struct vmm_alloc_node *)owner->last_vmm_node)->owner_next = new_node;
        } else {
            owner->first_vmm_node = new_node;
        }

        owner->last_vmm_node = new_node;
    }

    StRbtree_Insert(&ad->rbtree, &new_node->rbnode);

    ad->free_count -= count;

    LOG_TRACE(LM_CAT_UNCLASSIFIED, "allocated %zu pages at %013zX\n", count, new_node->base_vpn);

    *vpn = new_node->base_vpn;

    StThread_UnlockPreemption();

    return STATUS_SUCCESS;
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

    if (count == 0) return STATUS_INVALID_VALUE;
    if (vpn + count < vpn) return STATUS_INVALID_VALUE;

    ad = &alloc_domain_list[domain];
    if (!ad->initialized) return STATUS_CONFLICTING_STATE;

    StThread_LockPreemption();

    if (find_overlap(&ad->rbtree, vpn, count)) {
        status = STATUS_CONFLICTING_STATE;
        goto has_error;
    }

    status = create_alloc_node(&new_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    new_node->base_vpn = vpn;
    new_node->limit_vpn = vpn + count;
    new_node->owner = owner;
    new_node->asp = NULL;
    new_node->domain = domain;

    if (alloc_flags & AF_VMM_HIDDEN_AT_MAP) {
        new_node->alloc_type = AT_MAP;
    } else {
        new_node->alloc_type = AT_ALLOC;
    }

    if (owner) {
        new_node->owner_prev = owner->last_vmm_node;
        new_node->owner_next = NULL;

        if (owner->last_vmm_node) {
            ((struct vmm_alloc_node *)owner->last_vmm_node)->owner_next = new_node;
        } else {
            owner->first_vmm_node = new_node;
        }

        owner->last_vmm_node = new_node;
    }

    StRbtree_Insert(&ad->rbtree, &new_node->rbnode);
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
    struct StMm_AllocationOwner *owner = &asp->process->alloc_owner;

    if (count == 0) return STATUS_INVALID_VALUE;
    if (vpn + count < vpn) return STATUS_INVALID_VALUE;

    if (vpn < asp->user_base_vpn || (vpn + count - 1) > asp->user_limit_vpn) {
        return STATUS_INVALID_VALUE;
    }

    StThread_LockPreemption();

    if (find_overlap(&asp->user_rbtree, vpn, count)) {
        status = STATUS_CONFLICTING_STATE;
        goto has_error;
    }

    status = create_alloc_node(&new_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    new_node->base_vpn = vpn;
    new_node->limit_vpn = vpn + count;
    new_node->owner = owner;
    new_node->asp = asp;
    new_node->alloc_type = AT_ALLOC;

    new_node->owner_prev = owner->last_vmm_node;
    new_node->owner_next = NULL;

    if (owner->last_vmm_node) {
        ((struct vmm_alloc_node *)owner->last_vmm_node)->owner_next = new_node;
    } else {
        owner->first_vmm_node = new_node;
    }

    owner->last_vmm_node = new_node;

    StRbtree_Insert(&asp->user_rbtree, &new_node->rbnode);
    asp->user_free_count -= count;

    StThread_UnlockPreemption();

    return STATUS_SUCCESS;

has_error:
    StThread_UnlockPreemption();

    return status;
}

void StVmm_FreeGlobalPage(
    enum StVmm_Domain domain __in, St_VirtPage vpn __in, St_PageCount count __in
)
{
    struct vmm_alloc_domain *ad;
    struct vmm_alloc_node *node;

    if (count == 0) return;

    ad = &alloc_domain_list[domain];
    if (!ad->initialized) return;

    StThread_LockPreemption();

    node = find_overlap(&ad->rbtree, vpn, count);

    if (node && node->base_vpn == vpn && node->limit_vpn == vpn + count) {
        StRbtree_Delete(&ad->rbtree, &node->rbnode);

        // Unlink from owner list
        if (node->owner) {
            if (node->owner_prev) {
                node->owner_prev->owner_next = node->owner_next;
            } else {
                node->owner->first_vmm_node = node->owner_next;
            }

            if (node->owner_next) {
                node->owner_next->owner_prev = node->owner_prev;
            } else {
                node->owner->last_vmm_node = node->owner_prev;
            }
        }

        ad->free_count += count;

        LOG_TRACE(LM_CAT_UNCLASSIFIED, "freed %zu pages at %013zX\n", count, vpn);

        // TODO: Free the node structure itself when a proper allocator is available.
    } else {
        if (!node) {
            LOG_ERROR(LM_CAT_UNCLASSIFIED, "vmm node not found for vpn %013zX\n", vpn);
        } else {
            LOG_ERROR(
                LM_CAT_UNCLASSIFIED,
                "vmm node mismatch for vpn %013zX: found [%013zX, %013zX)\n",
                vpn,
                node->base_vpn,
                node->limit_vpn
            );
        }
    }

    StThread_UnlockPreemption();
}

void StVmm_FreeLocalPage(
    struct StMm_AddressSpace *asp __in, St_VirtPage vpn __in, St_PageCount count __in
)
{
    struct vmm_alloc_node *node;

    if (count == 0) return;
    if (vpn < asp->user_base_vpn || (vpn + count - 1) > asp->user_limit_vpn) return;

    StThread_LockPreemption();

    node = find_overlap(&asp->user_rbtree, vpn, count);

    if (node && node->base_vpn == vpn && node->limit_vpn == vpn + count) {
        StRbtree_Delete(&asp->user_rbtree, &node->rbnode);

        // Unlink from owner list
        if (node->owner) {
            if (node->owner_prev) {
                node->owner_prev->owner_next = node->owner_next;
            } else {
                node->owner->first_vmm_node = node->owner_next;
            }

            if (node->owner_next) {
                node->owner_next->owner_prev = node->owner_prev;
            } else {
                node->owner->last_vmm_node = node->owner_prev;
            }
        }

        asp->user_free_count += count;

        LOG_TRACE(LM_CAT_UNCLASSIFIED, "freed %zu pages at %013zX (local)\n", count, vpn);

        // TODO: Free the node structure itself when a proper allocator is available.
    }

    StThread_UnlockPreemption();
}
