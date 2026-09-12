#ifndef CLRS_HASH_OPEN_ADDRESS_H
#define CLRS_HASH_OPEN_ADDRESS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS 11.4 Open addressing with linear probing.
 * h(k,i) = (h'(k) + i) mod m
 * Slot empty = OPEN_EMPTY_KEY sentinel.
 */
#define OPEN_EMPTY_KEY ((int)0x80000000) /* INT_MIN */

typedef struct {
  int *keys;
  int *vals;
  size_t m;
  size_t n; /* occupied count */
} OpenHash;

void open_hash_init(OpenHash *t, size_t m);
void open_hash_destroy(OpenHash *t);

/* Insert or update (upsert): an existing key keeps its slot and gets the
 * new value; duplicates are never stored twice. */
void open_hash_insert(OpenHash *t, int key, int value);
int open_hash_search(const OpenHash *t, int key, int *value);

/* Delete with tombstone (DELETED key). */
int open_hash_delete(OpenHash *t, int key);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_HASH_OPEN_ADDRESS_H */
