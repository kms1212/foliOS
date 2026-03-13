#ifndef __STRATA_GNT_H__
#define __STRATA_GNT_H__

#include <stddef.h>

#include <strata/limits.h>
#include <strata/module.h>
#include <strata/status.h>
#include <strata/utf.h>

enum StGnt_NodeType {
    GNT_NODETYPE_LEAF = 0,
    GNT_NODETYPE_DIRECTORY,
    GNT_NODETYPE_LINK,
};

struct StGnt_Node {
    struct StGnt_Node *next;

    struct StGnt_Node *parent;
    struct StGnt_Node *sibling;

    size_t name_len;
    St_Utf32Char name[NODENAME_MAX];

    enum StGnt_NodeType type;

    union {
        struct {
            struct StModule *handler;
        } leaf;

        struct {
            struct StGnt_Node *children_head;
            struct StGnt_Node *children_tail;
            struct StModule *handler;
        } directory;

        struct {
            int is_virtual;

            struct {
                struct StGnt_Node *target_node;
            } virtual;

            struct {
                size_t target_path_len;
                St_Utf32Char *target_path;
            } physical;
        } link;
    };

    void *private_data;
};

extern struct StGnt_Node *g_gnt_root_network;  // "//"
extern struct StGnt_Node *g_gnt_root_local;    // "/"

StStatus StGnt_Init(void);
StStatus StGnt_AddNode(
    struct StGnt_Node *parent __in, const St_Utf32Char *name __in, struct StGnt_Node **node __out
);
StStatus StGnt_ResolveLink(
    struct StGnt_Node *link_node __in, struct StGnt_Node **target_node __out
);
StStatus StGnt_ResolvePath(
    struct StGnt_Node *base_node __in, const St_Utf32Char *path __in, struct StGnt_Node **node __out
);

#endif  // __STRATA_GNT_H__
