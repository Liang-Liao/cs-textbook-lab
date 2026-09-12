#include "bst.h"

#include <stdlib.h>

#include "clrs.h"

void bst_init(BST *t) { t->root = NULL; }

static void free_node(BSTNode *x) {
  if (x == NULL) {
    return;
  }
  free_node(x->left);
  free_node(x->right);
  free(x);
}

void bst_destroy(BST *t) {
  free_node(t->root);
  t->root = NULL;
}

BSTNode *bst_search(const BST *t, int key) {
  BSTNode *x = t->root;
  while (x != NULL && x->key != key) {
    x = (key < x->key) ? x->left : x->right;
  }
  return x;
}

BSTNode *bst_minimum(const BST *t) {
  BSTNode *x = t->root;
  if (x == NULL) {
    return NULL;
  }
  while (x->left != NULL) {
    x = x->left;
  }
  return x;
}

BSTNode *bst_maximum(const BST *t) {
  BSTNode *x = t->root;
  if (x == NULL) {
    return NULL;
  }
  while (x->right != NULL) {
    x = x->right;
  }
  return x;
}

BSTNode *bst_successor(BSTNode *x) {
  if (x == NULL) {
    return NULL;
  }
  if (x->right != NULL) {
    BSTNode *y = x->right;
    while (y->left != NULL) {
      y = y->left;
    }
    return y;
  }
  BSTNode *y = x->parent;
  while (y != NULL && x == y->right) {
    x = y;
    y = y->parent;
  }
  return y;
}

BSTNode *bst_predecessor(BSTNode *x) {
  if (x == NULL) {
    return NULL;
  }
  if (x->left != NULL) {
    BSTNode *y = x->left;
    while (y->right != NULL) {
      y = y->right;
    }
    return y;
  }
  BSTNode *y = x->parent;
  while (y != NULL && x == y->left) {
    x = y;
    y = y->parent;
  }
  return y;
}

BSTNode *bst_insert(BST *t, int key) {
  if (bst_search(t, key) != NULL) {
    return NULL;
  }
  BSTNode *z = clrs_xmalloc(sizeof(BSTNode));
  z->key = key;
  z->left = z->right = z->parent = NULL;

  BSTNode *y = NULL;
  BSTNode *x = t->root;
  while (x != NULL) {
    y = x;
    x = (key < x->key) ? x->left : x->right;
  }
  z->parent = y;
  if (y == NULL) {
    t->root = z;
  } else if (key < y->key) {
    y->left = z;
  } else {
    y->right = z;
  }
  return z;
}

/* CLRS TRANSPLANT: replace subtree rooted at u with v. */
static void transplant(BST *t, BSTNode *u, BSTNode *v) {
  if (u->parent == NULL) {
    t->root = v;
  } else if (u == u->parent->left) {
    u->parent->left = v;
  } else {
    u->parent->right = v;
  }
  if (v != NULL) {
    v->parent = u->parent;
  }
}

int bst_delete(BST *t, int key) {
  BSTNode *z = bst_search(t, key);
  if (z == NULL) {
    return 0;
  }

  if (z->left == NULL) {
    transplant(t, z, z->right);
  } else if (z->right == NULL) {
    transplant(t, z, z->left);
  } else {
    BSTNode *y = z->right;
    while (y->left != NULL) {
      y = y->left; /* successor */
    }
    if (y->parent != z) {
      transplant(t, y, y->right);
      y->right = z->right;
      y->right->parent = y;
    }
    transplant(t, z, y);
    y->left = z->left;
    y->left->parent = y;
  }
  free(z);
  return 1;
}

static size_t inorder_rec(const BSTNode *x, int *out, size_t maxn, size_t i) {
  if (x == NULL || i >= maxn) {
    return i;
  }
  i = inorder_rec(x->left, out, maxn, i);
  if (i < maxn) {
    out[i++] = x->key;
  }
  i = inorder_rec(x->right, out, maxn, i);
  return i;
}

size_t bst_inorder(const BST *t, int *out, size_t maxn) {
  return inorder_rec(t->root, out, maxn, 0);
}

static size_t size_rec(const BSTNode *x) {
  if (x == NULL) {
    return 0;
  }
  return 1 + size_rec(x->left) + size_rec(x->right);
}

size_t bst_size(const BST *t) { return size_rec(t->root); }
