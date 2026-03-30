#include <strata/gnt.h>

#include <string.h>

#include <strata/macros.h>
#include <strata/mm/pool.h>

static struct StGnt_Node *node_list_head = NULL;
static struct StGnt_Node *node_list_tail = NULL;

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

    if (parent) {
        if (parent->type == GNT_NODETYPE_LINK) {
            status = StGnt_ResolveLink(parent, &parent);
            if (!CHECK_SUCCESS(status)) goto has_error;
        }

        if (parent->type == GNT_NODETYPE_LEAF) {
            struct StModule *handler = parent->leaf.handler_module;

            parent->type = GNT_NODETYPE_DIRECTORY;
            parent->directory.handler_module = handler;
            parent->directory.children_head = parent->directory.children_tail = NULL;
        }

        if (!parent->directory.children_head) {
            parent->directory.children_head = parent->directory.children_tail = new_node;
        } else {
            parent->directory.children_tail->sibling = new_node;
            parent->directory.children_tail = new_node;
        }
    }

    if (!node_list_head) {
        node_list_head = node_list_tail = new_node;
    } else {
        node_list_tail->next = new_node;
        node_list_tail = new_node;
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
