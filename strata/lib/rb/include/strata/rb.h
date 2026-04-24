/*
 * Copyright (c) 2019 xieqing. https://github.com/xieqing
 * May be freely redistributed, but copyright notice must be retained.
 */

#ifndef __STRATA_RB_H__
#define __STRATA_RB_H__

#include <stddef.h>
#include <strata/status.h>

#define RB_DUP 1
#define RB_MIN 1

#define RED   0
#define BLACK 1

/* container_of implementation if not available */
#ifndef container_of
#    define container_of(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))
#endif

#define rb_entry(ptr, type, member) container_of(ptr, type, member)

enum StRbtree_TraversalType {
    PREORDER,
    INORDER,
    POSTORDER,
};

struct StRbtree_Node {
    struct StRbtree_Node *left;
    struct StRbtree_Node *right;
    struct StRbtree_Node *parent;
    char color;
};

/* Compare function now takes two nodes directly */
typedef int (*StRbtree_CompareFunc)(struct StRbtree_Node *node1, struct StRbtree_Node *node2);
typedef int (*StRbtree_ApplyFunc)(struct StRbtree_Node *node, void *cookie);
typedef void (*StRbtree_PrintFunc)(struct StRbtree_Node *node);

struct StRbtree {
    StRbtree_CompareFunc compare;
    StRbtree_PrintFunc print;

    struct StRbtree_Node root;
    struct StRbtree_Node nil;

#ifdef RB_MIN
    struct StRbtree_Node *min;
#endif
};

StStatus StRbtree_Create(struct StRbtree *rbt, StRbtree_CompareFunc compare_func);
/* Destroy no longer takes a function as it doesn't own nodes */
void StRbtree_Destroy(struct StRbtree *rbt);

/* Find now requires a comparable node key (can be a partial node or full node) */
struct StRbtree_Node *StRbtree_Find(
    struct StRbtree_Node *root,
    struct StRbtree_Node *nil,
    struct StRbtree_Node *key,
    StRbtree_CompareFunc compare
);

/* Wrapper for convenience if finding within a tree */
static inline struct StRbtree_Node *StRbtree_FindInTree(
    struct StRbtree *rbt, struct StRbtree_Node *key
)
{
    return StRbtree_Find(&rbt->root, &rbt->nil, key, rbt->compare);
}

struct StRbtree_Node *StRbtree_Successor(struct StRbtree *rbt, struct StRbtree_Node *node);

int StRbtree_ApplyNode(
    struct StRbtree *rbt,
    struct StRbtree_Node *node,
    StRbtree_ApplyFunc func,
    void *cookie,
    enum StRbtree_TraversalType order
);
void StRbtree_Print(struct StRbtree *rbt, StRbtree_PrintFunc print_func);

void StRbtree_Insert(struct StRbtree *rbt, struct StRbtree_Node *node);
void StRbtree_Delete(struct StRbtree *rbt, struct StRbtree_Node *node);

struct StRbtree_Node *StRbtree_Min(struct StRbtree *rbt);
struct StRbtree_Node *StRbtree_Max(struct StRbtree *rbt);

int StRbtree_CheckOrder(struct StRbtree *rbt);
int StRbtree_CheckBlackHeight(struct StRbtree *rbt);

#endif /* __STRATA_RB_H__ */
