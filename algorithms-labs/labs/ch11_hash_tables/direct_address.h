#ifndef CLRS_DIRECT_ADDRESS_H
#define CLRS_DIRECT_ADDRESS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS 11.1 Direct-address table.
 * Keys are in [0, universe-1]. Value 0 means empty; stored values are ints
 * (use nonzero sentinels for "present" if 0 is a valid payload).
 */
typedef struct {
  int *slot; /* length = universe; 0 = empty */
  size_t universe;
} DirectAddress;

void dad_init(DirectAddress *t, size_t universe);
void dad_destroy(DirectAddress *t);
void dad_insert(DirectAddress *t, size_t key, int value);
void dad_delete(DirectAddress *t, size_t key);
int dad_search(const DirectAddress *t, size_t key); /* 0 if absent */

#ifdef __cplusplus
}
#endif

#endif /* CLRS_DIRECT_ADDRESS_H */
