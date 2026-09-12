#include "hash_open_address.h"

#include <stdlib.h>

#include "clrs.h"
#include "hash_chain.h"

#define OPEN_DELETED_KEY ((int)0x7fffffff)

void open_hash_init(OpenHash *t, size_t m) {
  CLRS_ASSERT(m > 0, "m > 0");
  t->keys = clrs_xmalloc(m * sizeof(int));
  t->vals = clrs_xcalloc(m, sizeof(int));
  for (size_t i = 0; i < m; i++) {
    t->keys[i] = OPEN_EMPTY_KEY;
  }
  t->m = m;
  t->n = 0;
}

void open_hash_destroy(OpenHash *t) {
  free(t->keys);
  free(t->vals);
  t->keys = NULL;
  t->vals = NULL;
  t->m = 0;
  t->n = 0;
}

/* h'(k) = k mod m on non-negative encoding */
static size_t h1(int key, size_t m) { return hash_division(key, m); }

void open_hash_insert(OpenHash *t, int key, int value) {
  CLRS_ASSERT(key != OPEN_EMPTY_KEY && key != OPEN_DELETED_KEY,
              "key collides with sentinel");

  /*
   * CLRS 11.4 HASH-INSERT stops at the first EMPTY/DELETED slot and
   * assumes keys are distinct. This table offers upsert semantics, so it
   * must first scan the whole probe chain for the key: reusing an earlier
   * tombstone before finding the real entry would store the key twice.
   */
  size_t reuse = t->m; /* first DELETED slot on the chain, or m if none */
  size_t i = h1(key, t->m);
  for (size_t probe = 0; probe < t->m; probe++) {
    size_t j = (i + probe) % t->m;
    if (t->keys[j] == key) {
      t->vals[j] = value;
      return;
    }
    if (t->keys[j] == OPEN_DELETED_KEY && reuse == t->m) {
      reuse = j;
    }
    if (t->keys[j] == OPEN_EMPTY_KEY) {
      break; /* the key cannot lie past an EMPTY slot */
    }
  }
  if (reuse == t->m) {
    for (size_t probe = 0; probe < t->m; probe++) {
      size_t j = (i + probe) % t->m;
      if (t->keys[j] == OPEN_EMPTY_KEY) {
        reuse = j;
        break;
      }
    }
  }
  CLRS_ASSERT(reuse < t->m, "hash table overflow");
  t->keys[reuse] = key;
  t->vals[reuse] = value;
  t->n++;
}

int open_hash_search(const OpenHash *t, int key, int *value) {
  size_t i = h1(key, t->m);
  for (size_t probe = 0; probe < t->m; probe++) {
    size_t j = (i + probe) % t->m;
    if (t->keys[j] == OPEN_EMPTY_KEY) {
      return 0;
    }
    if (t->keys[j] == key) {
      if (value != NULL) {
        *value = t->vals[j];
      }
      return 1;
    }
  }
  return 0;
}

int open_hash_delete(OpenHash *t, int key) {
  size_t i = h1(key, t->m);
  for (size_t probe = 0; probe < t->m; probe++) {
    size_t j = (i + probe) % t->m;
    if (t->keys[j] == OPEN_EMPTY_KEY) {
      return 0;
    }
    if (t->keys[j] == key) {
      t->keys[j] = OPEN_DELETED_KEY;
      t->vals[j] = 0;
      t->n--;
      return 1;
    }
  }
  return 0;
}
