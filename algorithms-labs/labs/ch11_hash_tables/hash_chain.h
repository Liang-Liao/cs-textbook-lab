#ifndef CLRS_HASH_CHAIN_H
#define CLRS_HASH_CHAIN_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS 11.2 Hash table with chaining.
 * Keys and values are ints; value 0 is allowed (store separately).
 */
typedef struct ChainNode {
  int key;
  int value;
  struct ChainNode *next;
} ChainNode;

typedef struct {
  ChainNode **slots;
  size_t m; /* number of slots */
} ChainedHash;

/* Division method: h(k) = k mod m (works for non-negative keys). */
size_t hash_division(int key, size_t m);

/* Multiplication method (Knuth): floor(m * frac(k * A)), A = (sqrt(5)-1)/2. */
size_t hash_multiplication(int key, size_t m);

void chained_hash_init(ChainedHash *t, size_t m);
void chained_hash_destroy(ChainedHash *t);

/* Insert (allows duplicate keys — last wins on search of first match). */
void chained_hash_insert(ChainedHash *t, int key, int value);

/* Search: returns 1 and sets *value if found. */
int chained_hash_search(const ChainedHash *t, int key, int *value);

/* Delete one node with key. Returns 1 if deleted. */
int chained_hash_delete(ChainedHash *t, int key);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_HASH_CHAIN_H */
