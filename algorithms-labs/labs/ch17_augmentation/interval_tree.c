#include "interval_tree.h"

#include <limits.h>
#include <stdlib.h>

#include "clrs.h"

static void free_it(ITNode *x) {
  if (x == NULL) {
    return;
  }
  free_it(x->left);
  free_it(x->right);
  free(x);
}

static void update_max(ITNode *x) {
  if (x == NULL) {
    return;
  }
  int m = x->iv.high;
  if (x->left != NULL && x->left->max_high > m) {
    m = x->left->max_high;
  }
  if (x->right != NULL && x->right->max_high > m) {
    m = x->right->max_high;
  }
  x->max_high = m;
}

void itree_init(ITree *t) { t->root = NULL; }

void itree_destroy(ITree *t) {
  free_it(t->root);
  t->root = NULL;
}

ITNode *itree_insert(ITree *t, int low, int high) {
  CLRS_ASSERT(low <= high, "invalid interval");
  ITNode *z = clrs_xmalloc(sizeof(ITNode));
  z->iv.low = low;
  z->iv.high = high;
  z->max_high = high;
  z->left = z->right = z->parent = NULL;

  ITNode *y = NULL;
  ITNode *x = t->root;
  while (x != NULL) {
    y = x;
    x = (low < x->iv.low) ? x->left : x->right;
  }
  z->parent = y;
  if (y == NULL) {
    t->root = z;
  } else if (low < y->iv.low) {
    y->left = z;
  } else {
    y->right = z;
  }
  for (ITNode *u = z->parent; u != NULL; u = u->parent) {
    update_max(u);
  }
  return z;
}

static void recompute_max(ITNode *x) {
  if (x == NULL) {
    return;
  }
  recompute_max(x->left);
  recompute_max(x->right);
  update_max(x);
}

static void transplant(ITree *t, ITNode *u, ITNode *v) {
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

int itree_delete(ITree *t, int low) {
  ITNode *z = t->root;
  while (z != NULL && z->iv.low != low) {
    z = (low < z->iv.low) ? z->left : z->right;
  }
  if (z == NULL) {
    return 0;
  }
  if (z->left == NULL) {
    transplant(t, z, z->right);
  } else if (z->right == NULL) {
    transplant(t, z, z->left);
  } else {
    ITNode *y = z->right;
    while (y->left != NULL) {
      y = y->left;
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
  recompute_max(t->root);
  return 1;
}

static int overlaps(const Interval *a, int low, int high) {
  return a->low <= high && low <= a->high;
}

/* CLRS INTERVAL-SEARCH (one overlapping interval). */
ITNode *itree_search_overlap(const ITree *t, int low, int high) {
  ITNode *x = t->root;
  while (x != NULL && !overlaps(&x->iv, low, high)) {
    if (x->left != NULL && x->left->max_high >= low) {
      x = x->left;
    } else {
      x = x->right;
    }
  }
  return x;
}

static size_t count_rec(const ITNode *x, int low, int high) {
  if (x == NULL) {
    return 0;
  }
  size_t c = overlaps(&x->iv, low, high) ? 1 : 0;
  c += count_rec(x->left, low, high);
  c += count_rec(x->right, low, high);
  return c;
}

size_t itree_count_overlaps(const ITree *t, int low, int high) {
  return count_rec(t->root, low, high);
}
