#ifndef CLRS_BTREE_H
#define CLRS_BTREE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS Ch.18 B-tree of minimum degree t.
 * Keys are ints. Node has between t-1 and 2t-1 keys (except root).
 */

#define BTREE_MAX_KEYS (2 * 64 - 1) /* t <= 64 */

typedef struct BTreeNode {
  int leaf;
  int n; /* number of keys */
  int key[BTREE_MAX_KEYS];
  struct BTreeNode *child[BTREE_MAX_KEYS + 1];
} BTreeNode;

typedef struct {
  BTreeNode *root;
  int t; /* minimum degree, t >= 2 */
} BTree;

void btree_init(BTree *t, int min_degree);
void btree_destroy(BTree *t);

int btree_search(BTree *t, int key); /* 1 if found */
void btree_insert(BTree *t, int key);
int btree_delete(BTree *t, int key); /* 1 if deleted */

size_t btree_size(const BTree *t);
/* Collect all keys in order into out; returns count. */
size_t btree_inorder(const BTree *t, int *out, size_t maxn);

/* Max height (leaves at depth 0). */
int btree_height(const BTree *t);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_BTREE_H */
