#include <strata/gnt.h>

#include <assert.h>

#include <strata/compiler.h>
#include <strata/gnt/interface.h>
#include <strata/gnt_refs.h>
#include <strata/mm/pool.h>
#include <strata/ref_control.h>

static void free_interface_entries(StGnt_Node_InternalRef node)
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

void StGnt_AcquireNode(StGnt_Node_StrongRef node __inout)
{
    assert(node);

    StRefControlBlock_Acquire(&node->ref_control);
}

void StGnt_FinalizeNode(void *object __in)  // NOLINT(misc-no-recursion)
{
    StGnt_Node_InternalRef node = object;
    StGnt_Node_InternalRef child;

    assert(node);

    if (node->type != GNT_NODETYPE_LINK) {
        child = node->children_head;
        node->children_head = NULL;
        node->children_tail = NULL;

        while (child) {
            StGnt_Node_InternalRef next = child->sibling;

            child->parent = NULL;
            child->sibling = NULL;
            StGnt_ReleaseNode((StGnt_Node_StrongRef)child);
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

void StGnt_ReleaseNode(StGnt_Node_StrongRef node __inout)
{
    assert(node);

    (void)StRefControlBlock_Release(&node->ref_control);
}
