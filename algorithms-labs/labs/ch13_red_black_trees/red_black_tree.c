#include "red_black_tree.h"

#include <stdlib.h>

#include "clrs.h"

void rb_init(RBTree *t) {
  t->nil = clrs_xmalloc(sizeof(RBNode));
  t->nil->color = RB_BLACK;
  t->nil->key = 0;
  t->nil->left = t->nil->right = t->nil->parent = t->nil;
  t->root = t->nil;
}

static void free_sub(RBTree *t, RBNode *x) {
  if (x == t->nil) {
    return;
  }
  free_sub(t, x->left);
  free_sub(t, x->right);
  free(x);
}

void rb_destroy(RBTree *t) {
  free_sub(t, t->root);
  free(t->nil);
  t->nil = NULL;
  t->root = NULL;
}

RBNode *rb_search(const RBTree *t, int key) {
  RBNode *x = t->root;
  while (x != t->nil && x->key != key) {
    x = (key < x->key) ? x->left : x->right;
  }
  return (x == t->nil) ? NULL : x;
}

RBNode *rb_minimum(const RBTree *t) {
  RBNode *x = t->root;
  if (x == t->nil) {
    return NULL;
  }
  while (x->left != t->nil) {
    x = x->left;
  }
  return x;
}

RBNode *rb_maximum(const RBTree *t) {
  RBNode *x = t->root;
  if (x == t->nil) {
    return NULL;
  }
  while (x->right != t->nil) {
    x = x->right;
  }
  return x;
}

RBNode *rb_successor(RBTree *t, RBNode *x) {
  if (x == NULL || x == t->nil) {
    return NULL;
  }
  if (x->right != t->nil) {
    RBNode *y = x->right;
    while (y->left != t->nil) {
      y = y->left;
    }
    return y;
  }
  RBNode *y = x->parent;
  while (y != t->nil && x == y->right) {
    x = y;
    y = y->parent;
  }
  return (y == t->nil) ? NULL : y;
}

/* CLRS LEFT-ROTATE */
static void left_rotate(RBTree *t, RBNode *x) {
  RBNode *y = x->right;
  x->right = y->left;
  if (y->left != t->nil) {
    y->left->parent = x;
  }
  y->parent = x->parent;
  if (x->parent == t->nil) {
    t->root = y;
  } else if (x == x->parent->left) {
    x->parent->left = y;
  } else {
    x->parent->right = y;
  }
  y->left = x;
  x->parent = y;
}

/* CLRS RIGHT-ROTATE */
static void right_rotate(RBTree *t, RBNode *x) {
  RBNode *y = x->left;
  x->left = y->right;
  if (y->right != t->nil) {
    y->right->parent = x;
  }
  y->parent = x->parent;
  if (x->parent == t->nil) {
    t->root = y;
  } else if (x == x->parent->right) {
    x->parent->right = y;
  } else {
    x->parent->left = y;
  }
  y->right = x;
  x->parent = y;
}

/* CLRS RB-INSERT-FIXUP */
static void rb_insert_fixup(RBTree *t, RBNode *z) {
  while (z->parent->color == RB_RED) {
    if (z->parent == z->parent->parent->left) {
      RBNode *y = z->parent->parent->right; /* uncle */
      if (y->color == RB_RED) {
        z->parent->color = RB_BLACK;
        y->color = RB_BLACK;
        z->parent->parent->color = RB_RED;
        z = z->parent->parent;
      } else {
        if (z == z->parent->right) {
          z = z->parent;
          left_rotate(t, z);
        }
        z->parent->color = RB_BLACK;
        z->parent->parent->color = RB_RED;
        right_rotate(t, z->parent->parent);
      }
    } else {
      RBNode *y = z->parent->parent->left;
      if (y->color == RB_RED) {
        z->parent->color = RB_BLACK;
        y->color = RB_BLACK;
        z->parent->parent->color = RB_RED;
        z = z->parent->parent;
      } else {
        if (z == z->parent->left) {
          z = z->parent;
          right_rotate(t, z);
        }
        z->parent->color = RB_BLACK;
        z->parent->parent->color = RB_RED;
        left_rotate(t, z->parent->parent);
      }
    }
  }
  t->root->color = RB_BLACK;
}

RBNode *rb_insert(RBTree *t, int key) {
  if (rb_search(t, key) != NULL) {
    return NULL;
  }
  RBNode *z = clrs_xmalloc(sizeof(RBNode));
  z->key = key;
  z->left = z->right = z->parent = t->nil;
  z->color = RB_RED;

  RBNode *y = t->nil;
  RBNode *x = t->root;
  while (x != t->nil) {
    y = x;
    x = (key < x->key) ? x->left : x->right;
  }
  z->parent = y;
  if (y == t->nil) {
    t->root = z;
  } else if (key < y->key) {
    y->left = z;
  } else {
    y->right = z;
  }
  z->left = z->right = t->nil;
  rb_insert_fixup(t, z);
  return z;
}

static void rb_transplant(RBTree *t, RBNode *u, RBNode *v) {
  if (u->parent == t->nil) {
    t->root = v;
  } else if (u == u->parent->left) {
    u->parent->left = v;
  } else {
    u->parent->right = v;
  }
  v->parent = u->parent;
}

/* CLRS RB-DELETE-FIXUP */
static void rb_delete_fixup(RBTree *t, RBNode *x) {
  while (x != t->root && x->color == RB_BLACK) {
    if (x == x->parent->left) {
      RBNode *w = x->parent->right;
      if (w->color == RB_RED) {
        w->color = RB_BLACK;
        x->parent->color = RB_RED;
        left_rotate(t, x->parent);
        w = x->parent->right;
      }
      if (w->left->color == RB_BLACK && w->right->color == RB_BLACK) {
        w->color = RB_RED;
        x = x->parent;
      } else {
        if (w->right->color == RB_BLACK) {
          w->left->color = RB_BLACK;
          w->color = RB_RED;
          right_rotate(t, w);
          w = x->parent->right;
        }
        w->color = x->parent->color;
        x->parent->color = RB_BLACK;
        w->right->color = RB_BLACK;
        left_rotate(t, x->parent);
        x = t->root;
      }
    } else {
      RBNode *w = x->parent->left;
      if (w->color == RB_RED) {
        w->color = RB_BLACK;
        x->parent->color = RB_RED;
        right_rotate(t, x->parent);
        w = x->parent->left;
      }
      if (w->right->color == RB_BLACK && w->left->color == RB_BLACK) {
        w->color = RB_RED;
        x = x->parent;
      } else {
        if (w->left->color == RB_BLACK) {
          w->right->color = RB_BLACK;
          w->color = RB_RED;
          left_rotate(t, w);
          w = x->parent->left;
        }
        w->color = x->parent->color;
        x->parent->color = RB_BLACK;
        w->left->color = RB_BLACK;
        right_rotate(t, x->parent);
        x = t->root;
      }
    }
  }
  x->color = RB_BLACK;
}

int rb_delete(RBTree *t, int key) {
  RBNode *z = rb_search(t, key);
  if (z == NULL) {
    return 0;
  }

  RBNode *y = z;
  RBColor y_original = y->color;
  RBNode *x;

  if (z->left == t->nil) {
    x = z->right;
    rb_transplant(t, z, z->right);
  } else if (z->right == t->nil) {
    x = z->left;
    rb_transplant(t, z, z->left);
  } else {
    y = z->right;
    while (y->left != t->nil) {
      y = y->left;
    }
    y_original = y->color;
    x = y->right;
    if (y->parent == z) {
      x->parent = y;
    } else {
      rb_transplant(t, y, y->right);
      y->right = z->right;
      y->right->parent = y;
    }
    rb_transplant(t, z, y);
    y->left = z->left;
    y->left->parent = y;
    y->color = z->color;
  }
  free(z);
  if (y_original == RB_BLACK) {
    rb_delete_fixup(t, x);
  }
  return 1;
}

static size_t inorder_rec(const RBTree *t, RBNode *x, int *out, size_t maxn,
                          size_t i) {
  if (x == t->nil || i >= maxn) {
    return i;
  }
  i = inorder_rec(t, x->left, out, maxn, i);
  if (i < maxn) {
    out[i++] = x->key;
  }
  i = inorder_rec(t, x->right, out, maxn, i);
  return i;
}

size_t rb_inorder(const RBTree *t, int *out, size_t maxn) {
  return inorder_rec(t, t->root, out, maxn, 0);
}

static size_t size_rec(const RBTree *t, RBNode *x) {
  if (x == t->nil) {
    return 0;
  }
  return 1 + size_rec(t, x->left) + size_rec(t, x->right);
}

size_t rb_size(const RBTree *t) { return size_rec(t, t->root); }

/* Returns black-height, or -1 if property violated. */
static int validate_rec(const RBTree *t, RBNode *x, int *red_red) {
  if (x == t->nil) {
    return 1; /* black-height of empty = 1 */
  }
  if (x->color == RB_RED) {
    if (x->left->color == RB_RED || x->right->color == RB_RED) {
      *red_red = 1;
    }
  }
  int lh = validate_rec(t, x->left, red_red);
  int rh = validate_rec(t, x->right, red_red);
  if (lh < 0 || rh < 0 || lh != rh) {
    return -1;
  }
  return (x->color == RB_BLACK) ? lh + 1 : lh;
}

int rb_validate(const RBTree *t) {
  if (t->root == t->nil) {
    return 1;
  }
  if (t->root->color != RB_BLACK) {
    return 0;
  }
  if (t->nil->color != RB_BLACK) {
    return 0;
  }
  int red_red = 0;
  int bh = validate_rec(t, t->root, &red_red);
  return (bh > 0 && !red_red) ? 1 : 0;
}
