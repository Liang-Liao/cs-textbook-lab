#include "direct_address.h"

#include <stdlib.h>

#include "clrs.h"

void dad_init(DirectAddress *t, size_t universe) {
  CLRS_ASSERT(universe > 0, "universe > 0");
  t->slot = clrs_xcalloc(universe, sizeof(int));
  t->universe = universe;
}

void dad_destroy(DirectAddress *t) {
  free(t->slot);
  t->slot = NULL;
  t->universe = 0;
}

void dad_insert(DirectAddress *t, size_t key, int value) {
  CLRS_ASSERT(key < t->universe, "key out of universe");
  CLRS_ASSERT(value != 0, "value 0 means empty; use nonzero");
  t->slot[key] = value;
}

void dad_delete(DirectAddress *t, size_t key) {
  CLRS_ASSERT(key < t->universe, "key out of universe");
  t->slot[key] = 0;
}

int dad_search(const DirectAddress *t, size_t key) {
  CLRS_ASSERT(key < t->universe, "key out of universe");
  return t->slot[key];
}
