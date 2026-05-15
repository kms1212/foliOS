#include <strata/gnt.h>
#include <strata/gnt/path.h>

#include <assert.h>
#include <string.h>

#include <strata/compiler.h>
#include <strata/gnt_refs.h>
#include <strata/limits.h>
#include <strata/module.h>
#include <strata/status.h>
#include <strata/utf.h>

static StStatus resolve_link(
    StGnt_Node_InternalRef link_node __in,
    int *link_depth __inout,
    StGnt_Node_InternalRef *target_node __out
)
{
    assert(link_depth);
    assert(target_node);

    while (link_node->type == GNT_NODETYPE_LINK) {
        if (++*link_depth > NODELINK_MAX) return STATUS_TOO_MANY_LINKS;

        if (link_node->link.is_virtual) {
            if (link_node->link.virtual.target_node == link_node) return STATUS_TOO_MANY_LINKS;
            link_node = link_node->link.virtual.target_node;
        } else {
            return STATUS_NOT_IMPLEMENTED;
        }
    }

    *target_node = link_node;

    return STATUS_SUCCESS;
}

static StStatus invoke_resolver(
    StGnt_Node_StrongRef base_node __in,
    const St_Utf32Char *inner_path __in,
    StGnt_Node_StrongRef *next_node __out,
    const St_Utf32Char **remaining_path __out
)
{
    assert(next_node);
    assert(remaining_path);

    struct StModule *resolver_module;

    if (!base_node || base_node->type == GNT_NODETYPE_LINK) return STATUS_INVALID_VALUE;

    resolver_module = base_node->handler_module;
    if (!resolver_module || !resolver_module->resolve) return STATUS_NOT_SUPPORTED;

    return resolver_module->resolve(base_node, inner_path, next_node, remaining_path);
}

StStatus StGnt_ResolveLink(
    StGnt_Node_StrongRef link_node __in, StGnt_Node_StrongRef *target_node __out
)
{
    assert(target_node);

    StStatus status;
    StGnt_Node_InternalRef target;
    int link_depth = 0;

    status = resolve_link((StGnt_Node_InternalRef)link_node, &link_depth, &target);
    if (!CHECK_SUCCESS(status)) return status;

    *target_node = (StGnt_Node_StrongRef)target;

    return STATUS_SUCCESS;
}

StStatus StGnt_ResolvePath(
    StGnt_Node_StrongRef base_node __in,
    const St_Utf32Char *path __in,
    StGnt_Node_StrongRef *node __out
)
{
    assert(node);

    StStatus status;
    StGnt_Node_InternalRef current = (StGnt_Node_InternalRef)base_node;
    StGnt_Node_InternalRef link_target;
    StGnt_Node_StrongRef resolve_target;
    int link_depth = 0;
    struct StGnt_PathCursor cursor;

    if (path[0] == '/') {
        if (path[1] == '/') {
            current = (StGnt_Node_InternalRef)g_gnt_root_network;
            path += 2;
        } else {
            current = (StGnt_Node_InternalRef)g_gnt_root_local;
            path++;
        }
    } else if (!current) {
        return STATUS_INVALID_VALUE;
    }

    for (;;) {
        const St_Utf32Char *remaining_path = NULL;
        StGnt_Node_InternalRef child;
        int child_found = 0;
        struct StModule *resolver_module = NULL;

        StGntPath_Begin(&cursor, path);
        if (StGntPath_Next(&cursor)) {
            break;
        }

        path = StGntPath_Remaining(&cursor);

        if (StGntPath_IsDot(&cursor)) {
            continue;
        }
        if (StGntPath_IsDotDot(&cursor)) {
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

        resolver_module = current->handler_module;
        child = current->children_head;
        if (!resolver_module && !child) return STATUS_NOT_A_DIRECTORY;

        while (child) {
            if (child->name_len == cursor.token_len &&
                memcmp(child->name, cursor.token, cursor.token_len * sizeof(St_Utf32Char)) == 0) {
                current = child;
                child_found = 1;
                break;
            }
            child = child->sibling;
        }
        if (!child_found) {
            if (!resolver_module) return STATUS_ENTRY_NOT_FOUND;

            status = invoke_resolver(
                (StGnt_Node_StrongRef)current,
                cursor.token,
                &resolve_target,
                &remaining_path
            );
            if (!CHECK_SUCCESS(status)) return status;

            if (!resolve_target || !remaining_path) return STATUS_INVALID_VALUE;

            current = (StGnt_Node_InternalRef)resolve_target;
            path = remaining_path;
        }
    }

    if (current->type == GNT_NODETYPE_LINK) {
        status = resolve_link(current, &link_depth, &link_target);
        if (!CHECK_SUCCESS(status)) return status;

        current = link_target;
    }

    *node = (StGnt_Node_StrongRef)current;

    return STATUS_SUCCESS;
}
