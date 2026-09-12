#include "veb.h"

#include <math.h>
#include <stdlib.h>

#include "clrs.h"

static int int_sqrt(int u) {
  /* u is power of 2: sqrt = u^{1/2} also power of 2 */
  int s = 1;
  while (s * s < u) {
    s <<= 1;
  }
  return s;
}

static int high(const VEB *t, int x) {
  int s = int_sqrt(t->u);
  return x / s;
}

static int low(const VEB *t, int x) {
  int s = int_sqrt(t->u);
  return x % s;
}

static int index(const VEB *t, int i, int j) {
  int s = int_sqrt(t->u);
  return i * s + j;
}

VEB *veb_create(int u) {
  CLRS_ASSERT(u >= 2 && (u & (u - 1)) == 0, "u power of 2, u>=2");
  VEB *t = clrs_xcalloc(1, sizeof(VEB));
  t->u = u;
  t->min = VEB_NIL;
  t->max = VEB_NIL;
  int s = int_sqrt(u);
  if (u == 2) {
    t->summary = NULL;
    t->cluster = NULL;
  } else {
    t->summary = veb_create(s);
    t->cluster = clrs_xcalloc((size_t)s, sizeof(VEB *));
    for (int i = 0; i < s; i++) {
      t->cluster[i] = veb_create(s);
    }
  }
  return t;
}

void veb_destroy(VEB *t) {
  if (t == NULL) {
    return;
  }
  if (t->u > 2) {
    veb_destroy(t->summary);
    int s = int_sqrt(t->u);
    for (int i = 0; i < s; i++) {
      veb_destroy(t->cluster[i]);
    }
    free(t->cluster);
  }
  free(t);
}

static int empty(const VEB *t) { return t->min == VEB_NIL; }

int veb_member(const VEB *t, int x) {
  if (x < 0 || x >= t->u) {
    return 0;
  }
  if (empty(t)) {
    return 0;
  }
  if (x == t->min || x == t->max) {
    return 1;
  }
  if (t->u == 2) {
    return 0;
  }
  return veb_member(t->cluster[high(t, x)], low(t, x));
}

int veb_minimum(const VEB *t) { return empty(t) ? VEB_NIL : t->min; }
int veb_maximum(const VEB *t) { return empty(t) ? VEB_NIL : t->max; }

void veb_insert(VEB *t, int x) {
  CLRS_ASSERT(x >= 0 && x < t->u, "key out of universe");
  if (veb_member(t, x)) {
    return; /* duplicates ignored, matching bst_insert (first wins) */
  }
  if (empty(t)) {
    t->min = x;
    t->max = x;
    return;
  }
  if (x < t->min) {
    int tmp = x;
    x = t->min;
    t->min = tmp;
  }
  if (t->u > 2) {
    int h = high(t, x);
    if (empty(t->cluster[h])) {
      veb_insert(t->summary, h);
      t->cluster[h]->min = low(t, x);
      t->cluster[h]->max = low(t, x);
    } else {
      veb_insert(t->cluster[h], low(t, x));
    }
  }
  if (x > t->max) {
    t->max = x;
  }
}

int veb_successor(const VEB *t, int x) {
  if (empty(t) || x >= t->u) {
    return VEB_NIL;
  }
  if (t->u == 2) {
    if (x == 0 && t->max == 1) {
      return 1;
    }
    return VEB_NIL;
  }
  if (!empty(t) && t->min != VEB_NIL && x < t->min) {
    return t->min;
  }
  int h = high(t, x);
  int l = low(t, x);
  /* check current cluster for succ */
  if (t->cluster[h]->max != VEB_NIL && l < t->cluster[h]->max) {
    return index(t, h, veb_successor(t->cluster[h], l));
  }
  int succ_c = veb_successor(t->summary, h);
  if (succ_c == VEB_NIL) {
    return VEB_NIL;
  }
  return index(t, succ_c, t->cluster[succ_c]->min);
}

int veb_predecessor(const VEB *t, int x) {
  if (empty(t) || x < 0) {
    return VEB_NIL;
  }
  if (t->u == 2) {
    if (x == 1 && t->min == 0) {
      return 0;
    }
    return VEB_NIL;
  }
  if (t->max != VEB_NIL && x > t->max) {
    return t->max;
  }
  int h = high(t, x);
  int l = low(t, x);
  if (t->cluster[h]->min != VEB_NIL && l > t->cluster[h]->min) {
    return index(t, h, veb_predecessor(t->cluster[h], l));
  }
  int pred_c = veb_predecessor(t->summary, h);
  if (pred_c == VEB_NIL) {
    /* maybe x >= min of tree */
    if (t->min != VEB_NIL && x > t->min) {
      return t->min;
    }
    return VEB_NIL;
  }
  return index(t, pred_c, t->cluster[pred_c]->max);
}

int veb_delete(VEB *t, int x) {
  CLRS_ASSERT(x >= 0 && x < t->u, "key out of universe");
  if (empty(t)) {
    return 0;
  }
  if (!veb_member(t, x)) {
    return 0;
  }

  if (t->min == t->max) {
    t->min = VEB_NIL;
    t->max = VEB_NIL;
    return 1;
  }

  if (t->u == 2) {
    if (x == 0) {
      t->min = 1;
      t->max = 1;
    } else {
      t->min = 0;
      t->max = 0;
    }
    return 1;
  }

  /* if deleting min, need next min from cluster */
  if (x == t->min) {
    int first = t->summary->min;
    int new_min = index(t, first, t->cluster[first]->min);
    t->min = new_min;
    x = new_min;
  }

  int h = high(t, x);
  int l = low(t, x);
  veb_delete(t->cluster[h], l);
  if (t->cluster[h]->min == VEB_NIL) {
    veb_delete(t->summary, h);
    if (x == t->max) {
      int s = t->summary->max;
      if (s == VEB_NIL) {
        t->max = t->min;
      } else {
        t->max = index(t, s, t->cluster[s]->max);
      }
    }
  } else if (x == t->max) {
    t->max = index(t, h, t->cluster[h]->max);
  }
  return 1;
}

static size_t count_veb(const VEB *t) {
  if (t == NULL || empty(t)) {
    return 0;
  }
  if (t->u == 2) {
    size_t n = 0;
    if (t->min != VEB_NIL) {
      n++;
    }
    if (t->max != VEB_NIL && t->max != t->min) {
      n++;
    }
    return n;
  }
  /* min is never stored in a cluster; max may appear in a cluster. */
  size_t n = (t->min != VEB_NIL) ? 1 : 0;
  int s = int_sqrt(t->u);
  for (int i = 0; i < s; i++) {
    n += count_veb(t->cluster[i]);
  }
  return n;
}

size_t veb_size(const VEB *t) { return count_veb(t); }
