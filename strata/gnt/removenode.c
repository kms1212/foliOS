#include <strata/gnt.h>

#include <strata/compiler.h>
#include <strata/status.h>

StStatus StGnt_RemoveNode(struct StGnt_Node *node __in)
{
    struct StGnt_Node *parent;
    struct StGnt_Node *prev;

    if (!node) return STATUS_INVALID_VALUE;

    parent = node->parent;
    if (!parent) return STATUS_SUCCESS;
    if (parent->type == GNT_NODETYPE_LINK) return STATUS_CONFLICTING_STATE;

    prev = NULL;
    if (parent->children_head == node) {
        parent->children_head = node->sibling;
    } else {
        prev = parent->children_head;
        while (prev && prev->sibling != node) {
            prev = prev->sibling;
        }
        if (!prev) return STATUS_ENTRY_NOT_FOUND;

        prev->sibling = node->sibling;
    }

    if (parent->children_tail == node) {
        parent->children_tail = prev;
    }

    node->parent = NULL;
    node->sibling = NULL;
    StGnt_ReleaseNode(node);

    return STATUS_SUCCESS;
}
