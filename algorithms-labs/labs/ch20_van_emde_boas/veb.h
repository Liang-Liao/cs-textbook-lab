#ifndef CLRS_VEB_H
#define CLRS_VEB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS Ch.20 van Emde Boas tree.
 * Universe U = {0,1,...,u-1}, u must be a power of 2.
 * NIL sentinel = -1 for empty min/max.
 */

#define VEB_NIL (-1)

typedef struct VEB {
  int u;          /* universe size */
  int min;        /* VEB_NIL if empty */
  int max;        /* VEB_NIL if empty */
  struct VEB *summary;
  struct VEB **cluster; /* sqrt(u) clusters of size sqrt(u) */
} VEB;

/* Create tree with universe size u (power of 2, u >= 2). */
VEB *veb_create(int u);
void veb_destroy(VEB *t);

int veb_member(const VEB *t, int x);
int veb_minimum(const VEB *t); /* VEB_NIL if empty */
int veb_maximum(const VEB *t);

/* CLRS 20.1.3 / 20.2. Duplicates are ignored on insert (first wins). */
void veb_insert(VEB *t, int x);
int veb_delete(VEB *t, int x); /* 1 if existed */

int veb_successor(const VEB *t, int x); /* min element > x, or VEB_NIL */
int veb_predecessor(const VEB *t, int x); /* max element < x, or VEB_NIL */

size_t veb_size(const VEB *t);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_VEB_H */
