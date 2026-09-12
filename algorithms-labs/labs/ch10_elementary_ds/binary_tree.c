#include "binary_tree.h"

#include <stdlib.h>

#include "clrs.h"

TreeNode *tree_node_new(int key) {
  TreeNode *n = clrs_xmalloc(sizeof(TreeNode));
  n->key = key;
  n->left = NULL;
  n->right = NULL;
  return n;
}

void tree_free(TreeNode *root) {
  if (root == NULL) {
    return;
  }
  tree_free(root->left);
  tree_free(root->right);
  free(root);
}

static size_t inorder_rec(const TreeNode *r, int *out, size_t maxn, size_t i) {
  if (r == NULL || i >= maxn) {
    return i;
  }
  i = inorder_rec(r->left, out, maxn, i);
  if (i < maxn) {
    out[i++] = r->key;
  }
  i = inorder_rec(r->right, out, maxn, i);
  return i;
}

size_t tree_inorder(const TreeNode *root, int *out, size_t maxn) {
  return inorder_rec(root, out, maxn, 0);
}

static size_t preorder_rec(const TreeNode *r, int *out, size_t maxn, size_t i) {
  if (r == NULL || i >= maxn) {
    return i;
  }
  out[i++] = r->key;
  i = preorder_rec(r->left, out, maxn, i);
  i = preorder_rec(r->right, out, maxn, i);
  return i;
}

size_t tree_preorder(const TreeNode *root, int *out, size_t maxn) {
  return preorder_rec(root, out, maxn, 0);
}

static size_t postorder_rec(const TreeNode *r, int *out, size_t maxn,
                            size_t i) {
  if (r == NULL || i >= maxn) {
    return i;
  }
  i = postorder_rec(r->left, out, maxn, i);
  i = postorder_rec(r->right, out, maxn, i);
  if (i < maxn) {
    out[i++] = r->key;
  }
  return i;
}

size_t tree_postorder(const TreeNode *root, int *out, size_t maxn) {
  return postorder_rec(root, out, maxn, 0);
}

size_t tree_size(const TreeNode *root) {
  if (root == NULL) {
    return 0;
  }
  return 1 + tree_size(root->left) + tree_size(root->right);
}

size_t tree_height(const TreeNode *root) {
  if (root == NULL) {
    return 0;
  }
  size_t lh = tree_height(root->left);
  size_t rh = tree_height(root->right);
  return 1 + (lh > rh ? lh : rh);
}
