#include "hash_chain.h"

#include <math.h>
#include <stdlib.h>

#include "clrs.h"

size_t hash_division(int key, size_t m) {
  CLRS_ASSERT(m > 0, "m > 0");
  /* Map to non-negative for % */
  unsigned long k = (unsigned long)(long)key;
  return (size_t)(k % (unsigned long)m);
}

size_t hash_multiplication(int key, size_t m) {
  const double A = 0.6180339887498949; /* (sqrt(5)-1)/2 */
  double k = (double)(unsigned long)(long)key;
  double frac = k * A;
  frac -= floor(frac);
  return (size_t)(floor((double)m * frac));
}

void chained_hash_init(ChainedHash *t, size_t m) {
  CLRS_ASSERT(m > 0, "m > 0");
  t->slots = clrs_xcalloc(m, sizeof(ChainNode *));
  t->m = m;
}

void chained_hash_destroy(ChainedHash *t) {
  for (size_t i = 0; i < t->m; i++) {
    ChainNode *x = t->slots[i];
    while (x != NULL) {
      ChainNode *n = x->next;
      free(x);
      x = n;
    }
  }
  free(t->slots);
  t->slots = NULL;
  t->m = 0;
}

void chained_hash_insert(ChainedHash *t, int key, int value) {
  size_t i = hash_division(key, t->m);
  ChainNode *x = clrs_xmalloc(sizeof(ChainNode));
  x->key = key;
  x->value = value;
  x->next = t->slots[i];
  t->slots[i] = x;
}

int chained_hash_search(const ChainedHash *t, int key, int *value) {
  size_t i = hash_division(key, t->m);
  for (ChainNode *x = t->slots[i]; x != NULL; x = x->next) {
    if (x->key == key) {
      if (value != NULL) {
        *value = x->value;
      }
      return 1;
    }
  }
  return 0;
}

int chained_hash_delete(ChainedHash *t, int key) {
  size_t i = hash_division(key, t->m);
  ChainNode *x = t->slots[i];
  ChainNode *prev = NULL;
  while (x != NULL && x->key != key) {
    prev = x;
    x = x->next;
  }
  if (x == NULL) {
    return 0;
  }
  if (prev == NULL) {
    t->slots[i] = x->next;
  } else {
    prev->next = x->next;
  }
  free(x);
  return 1;
}
