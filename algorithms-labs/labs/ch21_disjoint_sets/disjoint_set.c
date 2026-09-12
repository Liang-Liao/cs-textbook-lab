#include "disjoint_set.h"

#include <stdlib.h>

#include "clrs.h"

void ds_init(DisjointSet *s, size_t n) {
  s->n = n;
  s->parent = clrs_xmalloc(n * sizeof(int));
  s->rank = clrs_xcalloc(n, sizeof(int));
  for (size_t i = 0; i < n; i++) {
    ds_make_set(s, i);
  }
}

void ds_destroy(DisjointSet *s) {
  free(s->parent);
  free(s->rank);
  s->parent = NULL;
  s->rank = NULL;
  s->n = 0;
}

void ds_make_set(DisjointSet *s, size_t x) {
  CLRS_ASSERT(x < s->n, "element out of range");
  s->parent[x] = (int)x;
  s->rank[x] = 0;
}

int ds_find(DisjointSet *s, size_t x) {
  CLRS_ASSERT(x < s->n, "element out of range");
  int xi = (int)x;
  if (s->parent[xi] != xi) {
    s->parent[xi] = ds_find(s, (size_t)s->parent[xi]); /* path compression */
  }
  return s->parent[xi];
}

int ds_union(DisjointSet *s, size_t x, size_t y) {
  int rx = ds_find(s, x);
  int ry = ds_find(s, y);
  if (rx == ry) {
    return 0;
  }
  /* union by rank */
  if (s->rank[rx] < s->rank[ry]) {
    s->parent[rx] = ry;
  } else if (s->rank[rx] > s->rank[ry]) {
    s->parent[ry] = rx;
  } else {
    s->parent[ry] = rx;
    s->rank[rx]++;
  }
  return 1;
}

int ds_connected(const DisjointSet *s, size_t x, size_t y) {
  CLRS_ASSERT(x < s->n && y < s->n, "element out of range");
  /* const find without path compression */
  int xi = (int)x;
  int yi = (int)y;
  while (s->parent[xi] != xi) {
    xi = s->parent[xi];
  }
  while (s->parent[yi] != yi) {
    yi = s->parent[yi];
  }
  return xi == yi;
}

size_t ds_connected_components(size_t n, const int *eu, const int *ev, size_t m,
                               int *comp) {
  DisjointSet s;
  ds_init(&s, n);
  for (size_t i = 0; i < m; i++) {
    CLRS_ASSERT(eu[i] >= 0 && (size_t)eu[i] < n && ev[i] >= 0 &&
                    (size_t)ev[i] < n,
                "edge endpoint out of range");
    ds_union(&s, (size_t)eu[i], (size_t)ev[i]);
  }

  /* map root -> new component id */
  int *root_id = clrs_xmalloc(n * sizeof(int));
  for (size_t i = 0; i < n; i++) {
    root_id[i] = -1;
  }
  int next = 0;
  for (size_t i = 0; i < n; i++) {
    int r = ds_find(&s, i);
    if (root_id[r] < 0) {
      root_id[r] = next++;
    }
    comp[i] = root_id[r];
  }
  free(root_id);
  ds_destroy(&s);
  return (size_t)next;
}
