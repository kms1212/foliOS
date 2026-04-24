#include <strata/gnt.h>

#include <stdatomic.h>

#include <strata/compiler.h>
#include <strata/gnt/interface.h>
#include <strata/mm/pool.h>

static void free_interface_entries(struct StGnt_Node *node)
{
    struct StGnt_NodeInterface *entry = node->interface_head;

    node->interface_head = NULL;
    node->interface_tail = NULL;

    while (entry) {
        struct StGnt_NodeInterface *next = entry->next;

        StPool_Free(entry);
        entry = next;
    }
}

void StGnt_AcquireNode(struct StGnt_Node *node __inout)
{
    if (!node) return;

    atomic_fetch_add_explicit(&node->ref_count, 1, memory_order_relaxed);
}

void StGnt_ReleaseNode(struct StGnt_Node *node __inout)  // NOLINT(misc-no-recursion)
{
    struct StGnt_Node *child;

    if (!node) return;

    if (atomic_fetch_sub_explicit(&node->ref_count, 1, memory_order_acq_rel) != 1) {
        return;
    }

    if (node->type != GNT_NODETYPE_LINK) {
        child = node->children_head;
        node->children_head = NULL;
        node->children_tail = NULL;

        while (child) {
            struct StGnt_Node *next = child->sibling;

            child->parent = NULL;
            child->sibling = NULL;
            StGnt_ReleaseNode(child);
            child = next;
        }
    } else if (
        node->type == GNT_NODETYPE_LINK && !node->link.is_virtual && node->link.physical.target_path
    ) {
        StPool_Free(node->link.physical.target_path);
        node->link.physical.target_path = NULL;
        node->link.physical.target_path_len = 0;
    }

    free_interface_entries(node);
    StPool_Free(node);
}
