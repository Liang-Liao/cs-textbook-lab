#include "btree.h"

#include <stdlib.h>

#include "clrs.h"

static BTreeNode *node_new(int leaf) {
  BTreeNode *x = clrs_xcalloc(1, sizeof(BTreeNode));
  x->leaf = leaf;
  x->n = 0;
  return x;
}

void btree_init(BTree *t, int min_degree) {
  CLRS_ASSERT(min_degree >= 2, "t >= 2");
  CLRS_ASSERT(2 * min_degree - 1 <= BTREE_MAX_KEYS,
              "min degree exceeds node capacity");
  t->t = min_degree;
  t->root = node_new(1);
}

static void node_free(BTreeNode *x) {
  if (x == NULL) {
    return;
  }
  if (!x->leaf) {
    for (int i = 0; i <= x->n; i++) {
      node_free(x->child[i]);
    }
  }
  free(x);
}

void btree_destroy(BTree *t) {
  node_free(t->root);
  t->root = NULL;
}

/* CLRS B-TREE-SEARCH */
static int search_node(BTreeNode *x, int key) {
  int i = 0;
  while (i < x->n && key > x->key[i]) {
    i++;
  }
  if (i < x->n && key == x->key[i]) {
    return 1;
  }
  if (x->leaf) {
    return 0;
  }
  return search_node(x->child[i], key);
}

int btree_search(BTree *t, int key) {
  if (t->root == NULL) {
    return 0;
  }
  return search_node(t->root, key);
}

/* CLRS B-TREE-SPLIT-CHILD */
static void split_child(BTreeNode *x, int i, int t) {
  BTreeNode *y = x->child[i];
  BTreeNode *z = node_new(y->leaf);
  int mid = t - 1;

  z->n = t - 1;
  for (int j = 0; j < t - 1; j++) {
    z->key[j] = y->key[j + t];
  }
  if (!y->leaf) {
    for (int j = 0; j < t; j++) {
      z->child[j] = y->child[j + t];
    }
  }
  y->n = t - 1;

  for (int j = x->n; j >= i + 1; j--) {
    x->child[j + 1] = x->child[j];
  }
  x->child[i + 1] = z;
  for (int j = x->n - 1; j >= i; j--) {
    x->key[j + 1] = x->key[j];
  }
  x->key[i] = y->key[mid];
  x->n++;
}

/* CLRS B-TREE-INSERT-NONFULL */
static void insert_nonfull(BTreeNode *x, int key, int t) {
  int i = x->n - 1;
  if (x->leaf) {
    while (i >= 0 && key < x->key[i]) {
      x->key[i + 1] = x->key[i];
      i--;
    }
    x->key[i + 1] = key;
    x->n++;
  } else {
    while (i >= 0 && key < x->key[i]) {
      i--;
    }
    i++;
    if (x->child[i]->n == 2 * t - 1) {
      split_child(x, i, t);
      if (key > x->key[i]) {
        i++;
      }
    }
    insert_nonfull(x->child[i], key, t);
  }
}

/* CLRS B-TREE-INSERT */
void btree_insert(BTree *t, int key) {
  if (btree_search(t, key)) {
    return; /* ignore duplicates */
  }
  int deg = t->t;
  BTreeNode *r = t->root;
  if (r->n == 2 * deg - 1) {
    BTreeNode *s = node_new(0);
    t->root = s;
    s->child[0] = r;
    split_child(s, 0, deg);
    insert_nonfull(s, key, deg);
  } else {
    insert_nonfull(r, key, deg);
  }
}

/* ---- Delete (simplified CLRS cases 1–3) ---- */

static int find_key(BTreeNode *x, int key) {
  int idx = 0;
  while (idx < x->n && x->key[idx] < key) {
    idx++;
  }
  return idx;
}

static void remove_from_leaf(BTreeNode *x, int idx) {
  for (int i = idx + 1; i < x->n; i++) {
    x->key[i - 1] = x->key[i];
  }
  x->n--;
}

static int get_pred(BTreeNode *x, int idx) {
  BTreeNode *cur = x->child[idx];
  while (!cur->leaf) {
    cur = cur->child[cur->n];
  }
  return cur->key[cur->n - 1];
}

static int get_succ(BTreeNode *x, int idx) {
  BTreeNode *cur = x->child[idx + 1];
  while (!cur->leaf) {
    cur = cur->child[0];
  }
  return cur->key[0];
}

static void borrow_from_prev(BTreeNode *x, int idx, int t) {
  CLRS_UNUSED(t);
  BTreeNode *c = x->child[idx];
  BTreeNode *sib = x->child[idx - 1];

  for (int i = c->n - 1; i >= 0; i--) {
    c->key[i + 1] = c->key[i];
  }
  if (!c->leaf) {
    for (int i = c->n; i >= 0; i--) {
      c->child[i + 1] = c->child[i];
    }
  }
  c->key[0] = x->key[idx - 1];
  if (!c->leaf) {
    c->child[0] = sib->child[sib->n];
  }
  x->key[idx - 1] = sib->key[sib->n - 1];
  c->n++;
  sib->n--;
}

static void borrow_from_next(BTreeNode *x, int idx, int t) {
  CLRS_UNUSED(t);
  BTreeNode *c = x->child[idx];
  BTreeNode *sib = x->child[idx + 1];

  c->key[c->n] = x->key[idx];
  if (!c->leaf) {
    c->child[c->n + 1] = sib->child[0];
  }
  x->key[idx] = sib->key[0];
  for (int i = 1; i < sib->n; i++) {
    sib->key[i - 1] = sib->key[i];
  }
  if (!sib->leaf) {
    for (int i = 1; i <= sib->n; i++) {
      sib->child[i - 1] = sib->child[i];
    }
  }
  c->n++;
  sib->n--;
}

static void merge_nodes(BTreeNode *x, int idx, int t) {
  CLRS_UNUSED(t);
  BTreeNode *c = x->child[idx];
  BTreeNode *sib = x->child[idx + 1];

  c->key[c->n] = x->key[idx];
  for (int i = 0; i < sib->n; i++) {
    c->key[c->n + 1 + i] = sib->key[i];
  }
  if (!c->leaf) {
    for (int i = 0; i <= sib->n; i++) {
      c->child[c->n + 1 + i] = sib->child[i];
    }
  }
  c->n += sib->n + 1;

  for (int i = idx + 1; i < x->n; i++) {
    x->key[i - 1] = x->key[i];
  }
  for (int i = idx + 2; i <= x->n; i++) {
    x->child[i - 1] = x->child[i];
  }
  x->n--;
  free(sib);
}

static void fill_child(BTreeNode *x, int idx, int t) {
  if (idx != 0 && x->child[idx - 1]->n >= t) {
    borrow_from_prev(x, idx, t);
  } else if (idx != x->n && x->child[idx + 1]->n >= t) {
    borrow_from_next(x, idx, t);
  } else {
    if (idx != x->n) {
      merge_nodes(x, idx, t);
    } else {
      merge_nodes(x, idx - 1, t);
    }
  }
}

static void remove_from_node(BTreeNode *x, int key, int t) {
  int idx = find_key(x, key);

  if (idx < x->n && x->key[idx] == key) {
    if (x->leaf) {
      remove_from_leaf(x, idx);
    } else {
      if (x->child[idx]->n >= t) {
        int pred = get_pred(x, idx);
        x->key[idx] = pred;
        remove_from_node(x->child[idx], pred, t);
      } else if (x->child[idx + 1]->n >= t) {
        int succ = get_succ(x, idx);
        x->key[idx] = succ;
        remove_from_node(x->child[idx + 1], succ, t);
      } else {
        merge_nodes(x, idx, t);
        remove_from_node(x->child[idx], key, t);
      }
    }
  } else {
    if (x->leaf) {
      return;
    }
    int last = (idx == x->n);
    if (x->child[idx]->n < t) {
      fill_child(x, idx, t);
    }
    if (last && idx > x->n) {
      remove_from_node(x->child[idx - 1], key, t);
    } else {
      remove_from_node(x->child[idx], key, t);
    }
  }
}

int btree_delete(BTree *t, int key) {
  if (!btree_search(t, key)) {
    return 0;
  }
  int deg = t->t;
  remove_from_node(t->root, key, deg);

  if (t->root->n == 0) {
    BTreeNode *old = t->root;
    if (!old->leaf) {
      t->root = old->child[0];
      old->child[0] = NULL;
    } else {
      t->root = node_new(1);
    }
    free(old);
  }
  return 1;
}

static size_t count_node(const BTreeNode *x) {
  if (x == NULL) {
    return 0;
  }
  size_t n = (size_t)x->n;
  if (!x->leaf) {
    for (int i = 0; i <= x->n; i++) {
      n += count_node(x->child[i]);
    }
  }
  return n;
}

size_t btree_size(const BTree *t) { return count_node(t->root); }

static size_t inorder_rec(const BTreeNode *x, int *out, size_t maxn, size_t k) {
  if (x == NULL || k >= maxn) {
    return k;
  }
  int i;
  for (i = 0; i < x->n; i++) {
    if (!x->leaf) {
      k = inorder_rec(x->child[i], out, maxn, k);
    }
    if (k < maxn) {
      out[k++] = x->key[i];
    }
  }
  if (!x->leaf) {
    k = inorder_rec(x->child[i], out, maxn, k);
  }
  return k;
}

size_t btree_inorder(const BTree *t, int *out, size_t maxn) {
  return inorder_rec(t->root, out, maxn, 0);
}

static int height_node(const BTreeNode *x) {
  int h = 0;
  while (x != NULL && !x->leaf) {
    h++;
    x = x->child[0];
  }
  return h;
}

int btree_height(const BTree *t) { return height_node(t->root); }
