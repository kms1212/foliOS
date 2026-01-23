#include "strata/status.h"
#include <strata/mm.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <rb.h>

#include <strata/arch/mmu.h>
#include <strata/arch/cpufeatures.h>
#include <strata/arch/intrinsics/register.h>
#include <strata/arch/intrinsics/invlpg.h>

#include <strata/macros.h>
#include <strata/panic.h>
#include <strata/log.h>

#define MODULE_NAME "vmm"

#define EARLY_ALLOC_NODE_POOL_COUNT 1024

struct alloc_domain {
    struct StRbtree rbtree;
    St_VirtPage base_vpn, limit_vpn;
    size_t free_count;
    int initialized;
};

struct alloc_node {
    struct StRbtree_Node rbnode;
    St_VirtPage base_vpn, limit_vpn;
    uint32_t flags;
};

static struct alloc_domain alloc_domain_list[VMM_DOMAIN_MAX] = {
    [VMM_DOMAIN_USER] = { .initialized = 0 },
    [VMM_DOMAIN_MODULE] = { .initialized = 0 },
    [VMM_DOMAIN_KERNEL_FAST] = { .initialized = 0 },
    [VMM_DOMAIN_KERNEL_SLOW] = { .initialized = 0 },
    [VMM_DOMAIN_IO] = { .initialized = 0 },
};

static struct alloc_node early_alloc_node_pool[EARLY_ALLOC_NODE_POOL_COUNT];
static int early_alloc_node_pool_used_count = 0;

static StStatus create_alloc_node(struct alloc_node **node)
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
    struct alloc_node *n1 = rb_entry(node1, struct alloc_node, rbnode);
    struct alloc_node *n2 = rb_entry(node2, struct alloc_node, rbnode);

    if (n1->base_vpn < n2->base_vpn) return -1;
    if (n1->base_vpn > n2->base_vpn) return 1;
    return 0;
}

StStatus StVmm_InitDomain(
    enum StVmm_Domain domain __in,
    St_VirtPage base_vpn __in,
    St_VirtPage limit_vpn __in
)
{
    struct alloc_domain *alloc_domain;

    if (domain >= VMM_DOMAIN_MAX) return STATUS_INVALID_VALUE;

    alloc_domain = &alloc_domain_list[domain];

    StRbtree_Create(&alloc_domain->rbtree, compare_alloc);
    alloc_domain->base_vpn = base_vpn;
    alloc_domain->limit_vpn = limit_vpn;
    alloc_domain->free_count = limit_vpn - base_vpn + 1;
    alloc_domain->initialized = 1;

    return STATUS_SUCCESS;
}

StStatus StVmm_GetTotalPageCount(
    enum StVmm_Domain domain __in,
    St_PageCount count __out
)
{
    struct alloc_domain *alloc_domain;

    if (domain >= VMM_DOMAIN_MAX) return STATUS_INVALID_VALUE;

    alloc_domain = &alloc_domain_list[domain];

    if (!alloc_domain->initialized) return STATUS_INVALID_VALUE;

    *count = alloc_domain->limit_vpn - alloc_domain->base_vpn + 1;

    return STATUS_SUCCESS;
}

StStatus StVmm_GetFreePageCount(
    enum StVmm_Domain domain,
    St_PageCount count __out
)
{
    struct alloc_domain *alloc_domain;

    if (domain >= VMM_DOMAIN_MAX) return STATUS_INVALID_VALUE;

    alloc_domain = &alloc_domain_list[domain];

    if (!alloc_domain->initialized) return STATUS_INVALID_VALUE;

    // TODO: traverse rbtree to count free pages
    *count = alloc_domain->free_count;

    return STATUS_SUCCESS;
}

StStatus StVmm_AllocatePage(
    enum StVmm_Domain domain __in,
    St_VirtPage vpn __out,
    St_PageCount count __in,
    StVmm_AllocFlags alloc_flags __in
)
{
    StStatus status;
    struct alloc_node *new_node;
    struct alloc_domain *ad;
    struct StRbtree_Node *curr_rb;
    St_VirtPage candidate_start;

    if (domain >= VMM_DOMAIN_MAX) return STATUS_INVALID_VALUE;
    ad = &alloc_domain_list[domain];
    if (!ad->initialized) return STATUS_CONFLICTING_STATE;
    if (count == 0) return STATUS_INVALID_VALUE;

    /* * [First-Fit Algorithm]
     * RBT는 주소 순으로 정렬되어 있으므로 In-Order 순회를 하며
     * 노드 사이의 간격(Hole)을 검사합니다.
     */
    
    curr_rb = StRbtree_Min(&ad->rbtree);
    candidate_start = ad->base_vpn;

    while (curr_rb != NULL) { // &ad->rbtree.nil 체크 필요 여부는 라이브러리 구현에 따름 (보통 NULL)
        struct alloc_node *curr_node = rb_entry(curr_rb, struct alloc_node, rbnode);

        // 1. 현재 노드 앞의 공간 확인
        // (candidate_start ~ curr_node->base_vpn 사이가 비어있음)
        if (curr_node->base_vpn > candidate_start) {
            size_t gap = curr_node->base_vpn - candidate_start;
            if (gap >= count) {
                goto found_hole; // 찾았다!
            }
        }

        // 2. 다음 검색 시작점은 현재 노드의 끝
        candidate_start = curr_node->limit_vpn;

        // 3. 다음 노드로 이동
        curr_rb = StRbtree_Successor(&ad->rbtree, curr_rb);
    }

    // 4. 마지막 노드 뒤 ~ 도메인 끝 공간 확인
    if (ad->limit_vpn > candidate_start) {
        size_t gap = ad->limit_vpn - candidate_start;
        if (gap >= count) {
            goto found_hole;
        }
    }

    return STATUS_INSUFFICIENT_MEMORY;

found_hole:
    // 5. 할당 메타데이터 생성 및 삽입
    status = create_alloc_node(&new_node);
    if (!CHECK_SUCCESS(status)) {
        return status;
    }

    new_node->base_vpn = candidate_start;
    new_node->limit_vpn = candidate_start + count;
    new_node->flags = alloc_flags;

    // RBT 삽입
    StRbtree_Insert(&ad->rbtree, &new_node->rbnode);

    ad->free_count -= count;

    LOG_TRACE("allocated %zu pages at %013zX\n", count, new_node->base_vpn);

    *vpn = new_node->base_vpn;

    return STATUS_SUCCESS;
}

void StVmm_FreePage(
    St_VirtPage vpn __in,
    St_PageCount count __in
)
{}
