/*
 * Copyright (c) 2019 xieqing. https://github.com/xieqing
 * May be freely redistributed, but copyright notice must be retained.
 */

#include "rb.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    struct StRbtree_Node node;
    char key;
} mydata;

int compare_func(struct StRbtree_Node *node1, struct StRbtree_Node *node2)
{
    mydata *d1 = rb_entry(node1, mydata, node);
    mydata *d2 = rb_entry(node2, mydata, node);
    return d1->key - d2->key;
}

void print_char_func(struct StRbtree_Node *node)
{
    mydata *d = rb_entry(node, mydata, node);
    printf("%c", d->key);
}

mydata *makedata(char key)
{
    mydata *d = malloc(sizeof(mydata));
    if (d) {
        d->key = key;
    }
    return d;
}

int main(int argc, char *argv[])
{
    struct StRbtree rbt;  // Allocate on stack

    /* create a red-black tree */
    /* Note: StRbtree_Create now takes pointer to existing struct, not double pointer alloc */
    if (StRbtree_Create(&rbt, compare_func) != STATUS_SUCCESS) {
        fprintf(stderr, "create red-black tree failed\n");
        return 1;
    }

    /* insert items */
    char a[] = {'R', 'E', 'D', 'S', 'O', 'X', 'C', 'U', 'B', 'T'};
    int i;
    mydata *data;
    for (i = 0; i < sizeof(a) / sizeof(a[0]); i++) {
        if ((data = makedata(a[i])) == NULL) {
            fprintf(stderr, "insert %c: out of memory\n", a[i]);
            break;
        }
        StRbtree_Insert(&rbt, &data->node);

        printf("insert %c", a[i]);
        StRbtree_Print(&rbt, print_char_func);
        printf("\n");
    }

    /* delete item */
    struct StRbtree_Node *node;
    mydata query;
    query.key = 'O';
    printf("delete %c", query.key);

    /* Find needs a node to compare. We can use a temporary stack node with key set */
    if ((node = StRbtree_FindInTree(&rbt, &query.node)) != NULL) {
        StRbtree_Delete(&rbt, node);
        // We must free the memory! StRbtree_Delete only unlinks.
        free(rb_entry(node, mydata, node));
    }
    StRbtree_Print(&rbt, print_char_func);

#ifdef RB_MIN
    while ((node = StRbtree_Min(&rbt))) {
        printf("\ndelete ");
        print_char_func(node);
        StRbtree_Delete(&rbt, node);
        StRbtree_Print(&rbt, print_char_func);
        free(rb_entry(node, mydata, node));
    }
#endif

    StRbtree_Destroy(&rbt);
    return 0;
}

/*
 * usage: gcc -I include rb_example.c rb.c && ./a.out
 */
