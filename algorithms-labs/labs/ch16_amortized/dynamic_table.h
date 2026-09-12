#ifndef CLRS_DYNAMIC_TABLE_H
#define CLRS_DYNAMIC_TABLE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS Ch.16/17 Amortized analysis — dynamic tables.
 * Table doubles when full; contracts to n/2 when n < size/4 (and size > 1).
 * Tracks operation counts for amortized-cost experiments.
 */
typedef struct {
  int *a;
  size_t size;  /* allocated slots */
  size_t n;     /* occupied slots */
  unsigned long inserts;
  unsigned long deletes;
  unsigned long resizes; /* number of table movements (expand+contract) */
} DynTable;

void dtab_init(DynTable *t);
void dtab_destroy(DynTable *t);
void dtab_push(DynTable *t, int x);
int dtab_pop(DynTable *t); /* requires n > 0 */
int dtab_at(const DynTable *t, size_t i);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_DYNAMIC_TABLE_H */
