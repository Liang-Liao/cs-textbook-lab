#ifndef CLRS_RED_BLACK_TREE_H
#define CLRS_RED_BLACK_TREE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS Ch.13 Red-black tree with sentinel nil (book style).
 * Keys are ints; duplicates ignored on insert.
 */

typedef enum { RB_BLACK = 0, RB_RED = 1 } RBColor;

typedef struct RBNode {
  int key;
  RBColor color;
  struct RBNode *left;
  struct RBNode *right;
  struct RBNode *parent;
} RBNode;

typedef struct {
  RBNode *nil;  /* shared sentinel, always black */
  RBNode *root;
} RBTree;

void rb_init(RBTree *t);
void rb_destroy(RBTree *t);

RBNode *rb_search(const RBTree *t, int key);
RBNode *rb_minimum(const RBTree *t);
RBNode *rb_maximum(const RBTree *t);
RBNode *rb_successor(RBTree *t, RBNode *x);

/* CLRS RB-INSERT. Returns new node, or NULL if key exists. */
RBNode *rb_insert(RBTree *t, int key);

/* Returns 1 if deleted. */
int rb_delete(RBTree *t, int key);

size_t rb_inorder(const RBTree *t, int *out, size_t maxn);
size_t rb_size(const RBTree *t);

/* Validate red-black properties; returns 1 if valid. */
int rb_validate(const RBTree *t);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_RED_BLACK_TREE_H */
