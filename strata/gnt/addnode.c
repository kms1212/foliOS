#include <strata/gnt.h>

#include <stdatomic.h>
#include <string.h>

#include <strata/compiler.h>
#include <strata/mm/pool.h>
#include <strata/status.h>
#include <strata/utf.h>

StStatus StGnt_AddNode(
    struct StGnt_Node *parent __in, const St_Utf32Char *name __in, struct StGnt_Node **node __out
)
{
    StStatus status;
    struct StGnt_Node *new_node;
    size_t name_len;

    status = StPool_AllocateClear(sizeof(*new_node), (void **)&new_node);
    if (!CHECK_SUCCESS(status)) goto has_error;

    new_node->parent = parent;
    atomic_init(&new_node->ref_count, 1);

    if (parent) {
        if (parent->type == GNT_NODETYPE_LINK) {
            status = StGnt_ResolveLink(parent, &parent);
            if (!CHECK_SUCCESS(status)) goto has_error;
        }

        if (!parent->children_head) {
            parent->children_head = parent->children_tail = new_node;
        } else {
            parent->children_tail->sibling = new_node;
            parent->children_tail = new_node;
        }
    }

    if (name) {
        status = StUtf_CountUtf32Chars(name, sizeof(new_node->name), &name_len);
        if (!CHECK_SUCCESS(status)) goto has_error;

        memcpy(new_node->name, name, name_len * sizeof(*new_node->name));
        new_node->name_len = name_len;
    } else {
        new_node->name_len = 0;
    }

    if (node) *node = new_node;

    return STATUS_SUCCESS;

has_error:
    return status;
}
