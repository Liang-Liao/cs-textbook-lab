#include "order_stat_tree.h"

#include <stdlib.h>

#include "clrs.h"

static size_t osz(const OSTNode *x) { return x ? x->size : 0; }

static void set_size(OSTNode *x) {
  if (x != NULL) {
    x->size = 1 + osz(x->left) + osz(x->right);
  }
}

void ost_init(OSTree *t) {
  t->nil = clrs_xmalloc(sizeof(OSTNode));
  t->nil->color = OS_BLACK;
  t->nil->key = 0;
  t->nil->size = 0;
  t->nil->left = t->nil->right = t->nil->parent = t->nil;
  t->root = t->nil;
}

static void free_sub(OSTree *t, OSTNode *x) {
  if (x == t->nil) {
    return;
  }
  free_sub(t, x->left);
  free_sub(t, x->right);
  free(x);
}

void ost_destroy(OSTree *t) {
  free_sub(t, t->root);
  free(t->nil);
  t->nil = NULL;
  t->root = NULL;
}

size_t ost_size(const OSTree *t) { return osz(t->root); }

OSTNode *ost_search(const OSTree *t, int key) {
  OSTNode *x = t->root;
  while (x != t->nil && x->key != key) {
    x = (key < x->key) ? x->left : x->right;
  }
  return (x == t->nil) ? NULL : x;
}

static void left_rotate(OSTree *t, OSTNode *x) {
  OSTNode *y = x->right;
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
  set_size(x);
  set_size(y);
}

static void right_rotate(OSTree *t, OSTNode *x) {
  OSTNode *y = x->left;
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
  set_size(x);
  set_size(y);
}

static void insert_fixup(OSTree *t, OSTNode *z) {
  while (z->parent->color == OS_RED) {
    if (z->parent == z->parent->parent->left) {
      OSTNode *y = z->parent->parent->right;
      if (y->color == OS_RED) {
        z->parent->color = OS_BLACK;
        y->color = OS_BLACK;
        z->parent->parent->color = OS_RED;
        z = z->parent->parent;
      } else {
        if (z == z->parent->right) {
          z = z->parent;
          left_rotate(t, z);
        }
        z->parent->color = OS_BLACK;
        z->parent->parent->color = OS_RED;
        right_rotate(t, z->parent->parent);
      }
    } else {
      OSTNode *y = z->parent->parent->left;
      if (y->color == OS_RED) {
        z->parent->color = OS_BLACK;
        y->color = OS_BLACK;
        z->parent->parent->color = OS_RED;
        z = z->parent->parent;
      } else {
        if (z == z->parent->left) {
          z = z->parent;
          right_rotate(t, z);
        }
        z->parent->color = OS_BLACK;
        z->parent->parent->color = OS_RED;
        left_rotate(t, z->parent->parent);
      }
    }
  }
  t->root->color = OS_BLACK;
}

OSTNode *ost_insert(OSTree *t, int key) {
  if (ost_search(t, key) != NULL) {
    return NULL;
  }
  OSTNode *z = clrs_xmalloc(sizeof(OSTNode));
  z->key = key;
  z->color = OS_RED;
  z->size = 1;
  z->left = z->right = z->parent = t->nil;

  OSTNode *y = t->nil;
  OSTNode *x = t->root;
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
  for (OSTNode *u = z->parent; u != t->nil; u = u->parent) {
    u->size++;
  }
  insert_fixup(t, z);
  return z;
}

static void transplant(OSTree *t, OSTNode *u, OSTNode *v) {
  if (u->parent == t->nil) {
    t->root = v;
  } else if (u == u->parent->left) {
    u->parent->left = v;
  } else {
    u->parent->right = v;
  }
  v->parent = u->parent;
}

static void delete_fixup(OSTree *t, OSTNode *x) {
  while (x != t->root && x->color == OS_BLACK) {
    if (x == x->parent->left) {
      OSTNode *w = x->parent->right;
      if (w->color == OS_RED) {
        w->color = OS_BLACK;
        x->parent->color = OS_RED;
        left_rotate(t, x->parent);
        w = x->parent->right;
      }
      if (w->left->color == OS_BLACK && w->right->color == OS_BLACK) {
        w->color = OS_RED;
        x = x->parent;
      } else {
        if (w->right->color == OS_BLACK) {
          w->left->color = OS_BLACK;
          w->color = OS_RED;
          right_rotate(t, w);
          w = x->parent->right;
        }
        w->color = x->parent->color;
        x->parent->color = OS_BLACK;
        w->right->color = OS_BLACK;
        left_rotate(t, x->parent);
        x = t->root;
      }
    } else {
      OSTNode *w = x->parent->left;
      if (w->color == OS_RED) {
        w->color = OS_BLACK;
        x->parent->color = OS_RED;
        right_rotate(t, x->parent);
        w = x->parent->left;
      }
      if (w->right->color == OS_BLACK && w->left->color == OS_BLACK) {
        w->color = OS_RED;
        x = x->parent;
      } else {
        if (w->left->color == OS_BLACK) {
          w->right->color = OS_BLACK;
          w->color = OS_RED;
          left_rotate(t, w);
          w = x->parent->left;
        }
        w->color = x->parent->color;
        x->parent->color = OS_BLACK;
        w->left->color = OS_BLACK;
        right_rotate(t, x->parent);
        x = t->root;
      }
    }
  }
  x->color = OS_BLACK;
}

int ost_delete(OSTree *t, int key) {
  OSTNode *z = ost_search(t, key);
  if (z == NULL) {
    return 0;
  }

  OSTNode *y = z;
  OSColor y_original = y->color;
  OSTNode *x;

  if (z->left == t->nil) {
    x = z->right;
    /* z's element disappears: every strict ancestor of z shrinks by 1 */
    for (OSTNode *u = z->parent; u != t->nil; u = u->parent) {
      u->size--;
    }
    transplant(t, z, z->right);
  } else if (z->right == t->nil) {
    x = z->left;
    for (OSTNode *u = z->parent; u != t->nil; u = u->parent) {
      u->size--;
    }
    transplant(t, z, z->left);
  } else {
    y = z->right;
    while (y->left != t->nil) {
      y = y->left;
    }
    y_original = y->color;
    x = y->right;
    /* y's key moves into z's position; z's element is gone. Nodes on the
     * old y-to-z path lose y from their subtrees; strict ancestors of z
     * lose z's element. Rotations in delete_fixup recompute the two
     * rotated nodes via set_size, keeping every size on the path exact. */
    if (y->parent != z) {
      for (OSTNode *u = y->parent; u != z; u = u->parent) {
        u->size--;
      }
    }
    for (OSTNode *u = z->parent; u != t->nil; u = u->parent) {
      u->size--;
    }
    if (y->parent == z) {
      x->parent = y;
    } else {
      transplant(t, y, y->right);
      y->right = z->right;
      y->right->parent = y;
    }
    transplant(t, z, y);
    y->left = z->left;
    y->left->parent = y;
    y->color = z->color;
    set_size(y);
  }
  free(z);
  if (y_original == OS_BLACK) {
    delete_fixup(t, x);
  }
  return 1;
}

OSTNode *ost_select(const OSTree *t, size_t i) {
  OSTNode *x = t->root;
  while (x != t->nil) {
    size_t r = osz(x->left);
    if (i == r) {
      return x;
    }
    if (i < r) {
      x = x->left;
    } else {
      i = i - r - 1;
      x = x->right;
    }
  }
  return NULL;
}

size_t ost_rank(const OSTree *t, int key) {
  OSTNode *x = ost_search(t, key);
  if (x == NULL) {
    return osz(t->root);
  }
  size_t r = osz(x->left);
  OSTNode *y = x;
  while (y->parent != t->nil) {
    if (y == y->parent->right) {
      r += 1 + osz(y->parent->left);
    }
    y = y->parent;
  }
  return r;
}

static int validate_rec(const OSTree *t, OSTNode *x, int *red_red, size_t *bh) {
  if (x == t->nil) {
    *bh = 1;
    return 1;
  }
  if (x->color == OS_RED) {
    if (x->left->color == OS_RED || x->right->color == OS_RED) {
      *red_red = 1;
    }
  }
  size_t lbh = 0, rbh = 0;
  if (!validate_rec(t, x->left, red_red, &lbh) ||
      !validate_rec(t, x->right, red_red, &rbh)) {
    return 0;
  }
  if (lbh != rbh) {
    return 0;
  }
  size_t expect = 1 + osz(x->left) + osz(x->right);
  if (x->size != expect) {
    return 0;
  }
  *bh = (x->color == OS_BLACK) ? lbh + 1 : lbh;
  return 1;
}

int ost_validate(const OSTree *t) {
  if (t->root == t->nil) {
    return 1;
  }
  if (t->root->color != OS_BLACK) {
    return 0;
  }
  int red_red = 0;
  size_t bh = 0;
  return validate_rec(t, t->root, &red_red, &bh) && !red_red;
}
