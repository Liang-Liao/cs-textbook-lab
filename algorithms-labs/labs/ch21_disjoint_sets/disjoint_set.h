#ifndef CLRS_DISJOINT_SET_H
#define CLRS_DISJOINT_SET_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS Ch.21 Disjoint-set forest.
 * Union by rank + path compression.
 * Elements are 0..n-1.
 */

typedef struct {
  int *parent;
  int *rank;
  size_t n;
} DisjointSet;

void ds_init(DisjointSet *s, size_t n);
void ds_destroy(DisjointSet *s);

/* CLRS MAKE-SET */
void ds_make_set(DisjointSet *s, size_t x);

/* CLRS FIND-SET with path compression. Returns representative. */
int ds_find(DisjointSet *s, size_t x);

/* CLRS UNION (by rank). Returns 1 if a merge happened. */
int ds_union(DisjointSet *s, size_t x, size_t y);

int ds_connected(const DisjointSet *s, size_t x, size_t y);

/*
 * Connected components of an undirected graph.
 * edges: pairs (u,v), m edges. n vertices.
 * comp[v] = component id in 0..k-1 (after renumber).
 * Returns number of components k.
 */
size_t ds_connected_components(size_t n, const int *eu, const int *ev,
                               size_t m, int *comp);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_DISJOINT_SET_H */
