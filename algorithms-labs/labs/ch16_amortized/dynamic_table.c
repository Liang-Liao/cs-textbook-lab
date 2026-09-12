#include "dynamic_table.h"

#include <stdlib.h>

#include "clrs.h"

void dtab_init(DynTable *t) {
  t->a = NULL;
  t->size = 0;
  t->n = 0;
  t->inserts = 0;
  t->deletes = 0;
  t->resizes = 0;
}

void dtab_destroy(DynTable *t) {
  free(t->a);
  t->a = NULL;
  t->size = 0;
  t->n = 0;
}

static void dtab_resize(DynTable *t, size_t new_size) {
  int *na = clrs_xmalloc((new_size ? new_size : 1) * sizeof(int));
  size_t m = t->n < new_size ? t->n : new_size;
  for (size_t i = 0; i < m; i++) {
    na[i] = t->a[i];
  }
  free(t->a);
  t->a = na;
  t->size = new_size;
  t->resizes += m; /* each element moved once */
}

void dtab_push(DynTable *t, int x) {
  if (t->n == t->size) {
    size_t ns = (t->size == 0) ? 1 : t->size * 2;
    dtab_resize(t, ns);
  }
  t->a[t->n++] = x;
  t->inserts++;
}

int dtab_pop(DynTable *t) {
  CLRS_ASSERT(t->n > 0, "table empty");
  int x = t->a[--t->n];
  t->deletes++;
  /* Contract when n < size/4 and size > 1 (CLRS expansion+contraction). */
  if (t->n > 0 && t->n * 4 <= t->size && t->size >= 2) {
    dtab_resize(t, t->size / 2);
  }
  return x;
}

int dtab_at(const DynTable *t, size_t i) {
  CLRS_ASSERT(i < t->n, "index out of range");
  return t->a[i];
}
