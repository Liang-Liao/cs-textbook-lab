#ifndef CLRS_BINARY_TREE_H
#define CLRS_BINARY_TREE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS 10.4 Rooted trees — binary tree with left/right child pointers.
 */
typedef struct TreeNode {
  int key;
  struct TreeNode *left;
  struct TreeNode *right;
} TreeNode;

TreeNode *tree_node_new(int key);
void tree_free(TreeNode *root);

/* Visit order writes keys into out; returns count written (max maxn). */
size_t tree_inorder(const TreeNode *root, int *out, size_t maxn);
size_t tree_preorder(const TreeNode *root, int *out, size_t maxn);
size_t tree_postorder(const TreeNode *root, int *out, size_t maxn);

size_t tree_size(const TreeNode *root);
/* Nodes on longest root-to-leaf path (leaf = 1; empty = 0). */
size_t tree_height(const TreeNode *root);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_BINARY_TREE_H */
