#include "fib_heap.h"

#include <stdlib.h>

#include "clrs.h"

static FibNode *node_new(int key) {
  FibNode *x = clrs_xmalloc(sizeof(FibNode));
  x->key = key;
  x->degree = 0;
  x->mark = 0;
  x->parent = NULL;
  x->child = NULL;
  x->left = x;
  x->right = x;
  return x;
}

static void list_insert(FibNode *pos, FibNode *y) {
  /* insert y after pos in circular list */
  y->left = pos;
  y->right = pos->right;
  pos->right->left = y;
  pos->right = y;
}

static void list_remove(FibNode *x) {
  x->left->right = x->right;
  x->right->left = x->left;
  x->left = x;
  x->right = x;
}

void fib_init(FibHeap *h) {
  h->min = NULL;
  h->n = 0;
}

static void free_node(FibNode *x) {
  if (x == NULL) {
    return;
  }
  FibNode *start = x;
  FibNode *cur = x;
  do {
    FibNode *next = cur->right;
    free_node(cur->child);
    free(cur);
    cur = next;
  } while (cur != start);
}

void fib_destroy(FibHeap *h) {
  free_node(h->min);
  h->min = NULL;
  h->n = 0;
}

size_t fib_size(const FibHeap *h) { return h->n; }

int fib_minimum(const FibHeap *h) {
  CLRS_ASSERT(h->min != NULL, "empty heap");
  return h->min->key;
}

FibNode *fib_insert(FibHeap *h, int key) {
  FibNode *x = node_new(key);
  if (h->min == NULL) {
    h->min = x;
  } else {
    list_insert(h->min, x);
    if (x->key < h->min->key) {
      h->min = x;
    }
  }
  h->n++;
  return x;
}

void fib_union(FibHeap *h1, FibHeap *h2) {
  if (h2->min == NULL) {
    return;
  }
  if (h1->min == NULL) {
    h1->min = h2->min;
  } else {
    /* splice circular lists */
    FibNode *a = h1->min;
    FibNode *b = h2->min;
    FibNode *ar = a->right;
    FibNode *bl = b->left;
    a->right = b;
    b->left = a;
    ar->left = bl;
    bl->right = ar;
    if (h2->min->key < h1->min->key) {
      h1->min = h2->min;
    }
  }
  h1->n += h2->n;
  h2->min = NULL;
  h2->n = 0;
}

static void consolidate(FibHeap *h) {
  if (h->min == NULL) {
    return;
  }
  size_t max_deg = 64;
  FibNode **A = clrs_xcalloc(max_deg, sizeof(FibNode *));

  /* Snapshot root list */
  size_t cap = h->n + 1;
  FibNode **roots = clrs_xmalloc(cap * sizeof(FibNode *));
  size_t nr = 0;
  {
    FibNode *w = h->min;
    do {
      roots[nr++] = w;
      w = w->right;
    } while (w != h->min && nr < cap);
  }

  /* Unlink all from circular list by resetting to singleton */
  for (size_t i = 0; i < nr; i++) {
    roots[i]->left = roots[i];
    roots[i]->right = roots[i];
    roots[i]->parent = NULL;
  }

  for (size_t i = 0; i < nr; i++) {
    FibNode *x = roots[i];
    size_t d = (size_t)x->degree;
    while (A[d] != NULL) {
      FibNode *y = A[d];
      if (x->key > y->key) {
        FibNode *tmp = x;
        x = y;
        y = tmp;
      }
      /* link y under x: y is singleton or we handle via fib_link */
      if (x->child == NULL) {
        x->child = y;
        y->left = y->right = y;
      } else {
        list_insert(x->child, y);
      }
      y->parent = x;
      x->degree++;
      y->mark = 0;
      A[d] = NULL;
      d++;
    }
    A[d] = x;
  }

  h->min = NULL;
  for (size_t d = 0; d < max_deg; d++) {
    if (A[d] != NULL) {
      A[d]->parent = NULL;
      if (h->min == NULL) {
        A[d]->left = A[d]->right = A[d];
        h->min = A[d];
      } else {
        list_insert(h->min, A[d]);
        if (A[d]->key < h->min->key) {
          h->min = A[d];
        }
      }
    }
  }

  free(roots);
  free(A);
}

int fib_extract_min(FibHeap *h) {
  FibNode *z = h->min;
  CLRS_ASSERT(z != NULL, "empty heap");
  int key = z->key;

  /* Promote children to root list */
  if (z->child != NULL) {
    size_t cap = h->n + 1;
    FibNode **kids = clrs_xmalloc(cap * sizeof(FibNode *));
    size_t nk = 0;
    FibNode *c = z->child;
    do {
      kids[nk++] = c;
      c = c->right;
    } while (c != z->child && nk < cap);
    for (size_t i = 0; i < nk; i++) {
      kids[i]->parent = NULL;
      /* CLRS keeps stale marks here; CASCADING-CUT's parent guard makes
       * marked roots harmless. */
      kids[i]->left = kids[i];
      kids[i]->right = kids[i];
      list_insert(z, kids[i]); /* insert into root list near z */
    }
    free(kids);
    z->child = NULL;
  }

  if (z->right == z) {
    /* only root */
    h->min = NULL;
  } else {
    FibNode *next = z->right;
    list_remove(z);
    h->min = next;
  }

  free(z);
  h->n--;

  if (h->min != NULL) {
    consolidate(h);
  }

  return key;
}

static void cut(FibHeap *h, FibNode *x, FibNode *y) {
  if (x->right == x) {
    y->child = NULL;
  } else {
    if (y->child == x) {
      y->child = x->right;
    }
    list_remove(x);
  }
  y->degree--;
  x->parent = NULL;
  x->mark = 0;
  list_insert(h->min, x);
}

void fib_decrease_key(FibHeap *h, FibNode *x, int new_key) {
  CLRS_ASSERT(new_key <= x->key, "new key greater than current");
  x->key = new_key;
  FibNode *y = x->parent;
  if (y != NULL && x->key < y->key) {
    cut(h, x, y);
    /* CLRS CASCADING-CUT: only walk up while y has a parent (z != NIL);
     * roots are never cut nor marked. */
    while (y->parent != NULL) {
      if (!y->mark) {
        y->mark = 1;
        break;
      }
      FibNode *z = y->parent;
      cut(h, y, z);
      y = z;
    }
  }
  if (h->min == NULL || x->key < h->min->key) {
    h->min = x;
  }
}

size_t fib_collect(const FibHeap *h, int *out, size_t maxn) {
  if (h->min == NULL) {
    return 0;
  }
  size_t cap = h->n + 1;
  FibNode **st = clrs_xmalloc(cap * sizeof(FibNode *));
  size_t sp = 0;
  size_t n = 0;

  FibNode *r = h->min;
  do {
    st[sp++] = r;
    r = r->right;
  } while (r != h->min && sp < cap);

  while (sp > 0 && n < maxn) {
    FibNode *x = st[--sp];
    out[n++] = x->key;
    if (x->child != NULL) {
      FibNode *c = x->child;
      do {
        if (sp < cap) {
          st[sp++] = c;
        }
        c = c->right;
      } while (c != x->child && sp < cap);
    }
  }
  free(st);
  return n;
}
