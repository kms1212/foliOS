#include <strata/gnt.h>

#include <strata/limits.h>
#include <strata/module.h>
#include <strata/utf.h>
#include <string.h>

static int advance_token(
    const St_Utf32Char *path __in, const St_Utf32Char **next_token __out, size_t *next_len __out
)
{
    while (*path == '/') {
        path++;
    }
    if (*path == '\0') return 1;

    const St_Utf32Char *token_start = path;
    size_t len = 0;

    while (*path != '/' && *path != '\0') {
        if (len < NODENAME_MAX) {
            len++;
        }
        path++;
    }

    if (next_token) *next_token = token_start;
    if (next_len) *next_len = len;

    return 0;
}

static StStatus resolve_link(
    struct StGnt_Node *link_node __in,
    int *link_depth __inout,
    struct StGnt_Node **target_node __out
)
{
    while (link_node->type == GNT_NODETYPE_LINK) {
        if (++*link_depth > NODELINK_MAX) return STATUS_TOO_MANY_LINKS;

        if (link_node->link.is_virtual) {
            if (link_node->link.virtual.target_node == link_node) return STATUS_TOO_MANY_LINKS;
            link_node = link_node->link.virtual.target_node;
        } else {
            return STATUS_NOT_IMPLEMENTED;
        }
    }

    if (target_node) *target_node = link_node;

    return STATUS_SUCCESS;
}

static StStatus invoke_resolver(
    struct StGnt_Node *base_node __in,
    const St_Utf32Char *inner_path __in,
    struct StGnt_Node **next_node __out,
    const St_Utf32Char **remaining_path __out
)
{
    return STATUS_NOT_IMPLEMENTED;
}

StStatus StGnt_ResolveLink(struct StGnt_Node *link_node __in, struct StGnt_Node **target_node __out)
{
    int link_depth = 0;
    return resolve_link(link_node, &link_depth, target_node);
}

StStatus StGnt_ResolvePath(
    struct StGnt_Node *base_node __in, const St_Utf32Char *path __in, struct StGnt_Node **node __out
)
{
    StStatus status;
    struct StGnt_Node *current = base_node, *link_target, *resolve_target;
    int link_depth = 0;

    if (path[0] == '/') {
        if (path[1] == '/') {
            current = g_gnt_root_network;
            path += 2;
        } else {
            current = g_gnt_root_local;
            path++;
        }
    } else if (!current) {
        return STATUS_INVALID_VALUE;
    }

    for (;;) {
        const St_Utf32Char *token_start;
        size_t token_len;
        struct StGnt_Node *child;
        int child_found = 0;
        struct StModule *resolver_module;

        if (advance_token(path, &token_start, &token_len)) {
            break;
        }

        path = token_start + token_len;

        if (token_len == 1 && token_start[0] == '.') {
            continue;
        }
        if (token_len == 2 && token_start[0] == '.' && token_start[1] == '.') {
            if (current->parent) {
                current = current->parent;
            }
            continue;
        }

        if (current->type == GNT_NODETYPE_LINK) {
            status = resolve_link(current, &link_depth, &link_target);
            if (!CHECK_SUCCESS(status)) return status;

            current = link_target;
        }

        if (current->type == GNT_NODETYPE_LEAF) {
            resolver_module = current->leaf.handler;
        } else if (current->type == GNT_NODETYPE_DIRECTORY) {
            resolver_module = current->directory.handler;
        }

        if (current->type != GNT_NODETYPE_DIRECTORY) {
            if (!resolver_module) return STATUS_NOT_A_DIRECTORY;

            status = invoke_resolver(current, token_start, &resolve_target, &token_start);
            if (!CHECK_SUCCESS(status)) return status;
        }

        child = current->directory.children_head;
        while (child) {
            if (child->name_len == token_len &&
                memcmp(child->name, token_start, token_len * sizeof(St_Utf32Char)) == 0) {
                current = child;
                child_found = 1;
                break;
            }
            child = child->sibling;
        }
        if (!child_found) {
            if (!resolver_module) return STATUS_NOT_A_DIRECTORY;

            status = invoke_resolver(current, token_start, &resolve_target, &token_start);
            if (!CHECK_SUCCESS(status)) return status;
        }
    }

    if (current->type == GNT_NODETYPE_LINK) {
        status = resolve_link(current, &link_depth, &link_target);
        if (!CHECK_SUCCESS(status)) return status;

        current = link_target;
    }

    if (node) *node = current;

    return STATUS_SUCCESS;
}
