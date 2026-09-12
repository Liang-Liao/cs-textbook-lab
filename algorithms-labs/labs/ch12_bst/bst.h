#ifndef CLRS_BST_H
#define CLRS_BST_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS Ch.12 Binary search tree with parent pointers.
 * Keys are ints; duplicates are ignored on insert (first wins).
 */
typedef struct BSTNode {
  int key;
  struct BSTNode *left;
  struct BSTNode *right;
  struct BSTNode *parent;
} BSTNode;

typedef struct {
  BSTNode *root; /* may be NULL */
} BST;

void bst_init(BST *t);
void bst_destroy(BST *t);

/* CLRS TREE-SEARCH (iterative). Returns node or NULL. */
BSTNode *bst_search(const BST *t, int key);

BSTNode *bst_minimum(const BST *t);
BSTNode *bst_maximum(const BST *t);

/* CLRS TREE-SUCCESSOR / TREE-PREDECESSOR. NULL if none. */
BSTNode *bst_successor(BSTNode *x);
BSTNode *bst_predecessor(BSTNode *x);

/* CLRS TREE-INSERT. Returns new node, or NULL if key already present. */
BSTNode *bst_insert(BST *t, int key);

/* CLRS TREE-DELETE. Returns 1 if key was found and deleted. */
int bst_delete(BST *t, int key);

/* Inorder keys into out[0..maxn); returns count. */
size_t bst_inorder(const BST *t, int *out, size_t maxn);

size_t bst_size(const BST *t);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_BST_H */
