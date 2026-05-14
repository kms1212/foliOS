#include <strata/gnt.h>

#include <assert.h>

#include <strata/compiler.h>
#include <strata/gnt_refs.h>

void StGnt_RemoveNode(StGnt_Node_StrongRef node __in)
{
    assert(node);

    StGnt_Node_InternalRef parent;
    StGnt_Node_InternalRef prev;

    parent = node->parent;
    if (!parent) return;
    assert(parent->type != GNT_NODETYPE_LINK);

    prev = NULL;
    if (parent->children_head == node) {
        parent->children_head = node->sibling;
    } else {
        prev = parent->children_head;
        while (prev && prev->sibling != node) {
            prev = prev->sibling;
        }
        assert(prev);
        if (!prev) return;

        prev->sibling = node->sibling;
    }

    if (parent->children_tail == node) {
        parent->children_tail = prev;
    }

    node->parent = NULL;
    node->sibling = NULL;
    StGnt_ReleaseNode(node);
}
