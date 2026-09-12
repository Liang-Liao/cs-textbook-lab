#include <stdio.h>
#include <stdlib.h>

#include "array.h"
#include "btree.h"
#include "clrs.h"
#include "test.h"

int main(void) {
  TestSuite t;
  test_init(&t);

  /* empty */
  {
    BTree bt;
    btree_init(&bt, 3);
    ASSERT_EQ_INT(&t, (int)btree_size(&bt), 0);
    ASSERT_TRUE(&t, !btree_search(&bt, 1));
    ASSERT_TRUE(&t, !btree_delete(&bt, 1));
    btree_destroy(&bt);
  }

  /* sequential insert + inorder sorted */
  {
    BTree bt;
    btree_init(&bt, 3); /* t=3, max 5 keys/node */
    for (int i = 1; i <= 20; i++) {
      btree_insert(&bt, i);
      ASSERT_TRUE(&t, btree_search(&bt, i));
    }
    ASSERT_EQ_INT(&t, (int)btree_size(&bt), 20);
    int out[32];
    size_t n = btree_inorder(&bt, out, 32);
    ASSERT_EQ_INT(&t, (int)n, 20);
    ASSERT_TRUE(&t, array_is_sorted_int(out, n));
    for (int i = 1; i <= 20; i++) {
      ASSERT_EQ_INT(&t, out[i - 1], i);
    }
    /* duplicates ignored */
    btree_insert(&bt, 10);
    ASSERT_EQ_INT(&t, (int)btree_size(&bt), 20);
    btree_destroy(&bt);
  }

  /* reverse order */
  {
    BTree bt;
    btree_init(&bt, 2);
    for (int i = 50; i >= 1; i--) {
      btree_insert(&bt, i);
    }
    int out[64];
    size_t n = btree_inorder(&bt, out, 64);
    ASSERT_EQ_INT(&t, (int)n, 50);
    ASSERT_TRUE(&t, array_is_sorted_int(out, n));
    ASSERT_TRUE(&t, btree_search(&bt, 1));
    ASSERT_TRUE(&t, btree_search(&bt, 50));
    ASSERT_TRUE(&t, !btree_search(&bt, 51));
    btree_destroy(&bt);
  }

  /* delete half */
  {
    BTree bt;
    btree_init(&bt, 3);
    for (int i = 0; i < 30; i++) {
      btree_insert(&bt, i);
    }
    for (int i = 0; i < 30; i += 2) {
      ASSERT_TRUE(&t, btree_delete(&bt, i));
    }
    ASSERT_EQ_INT(&t, (int)btree_size(&bt), 15);
    for (int i = 0; i < 30; i++) {
      if (i % 2 == 0) {
        ASSERT_TRUE(&t, !btree_search(&bt, i));
      } else {
        ASSERT_TRUE(&t, btree_search(&bt, i));
      }
    }
    int out[32];
    size_t n = btree_inorder(&bt, out, 32);
    ASSERT_TRUE(&t, array_is_sorted_int(out, n));
    btree_destroy(&bt);
  }

  /* random stress */
  {
    BTree bt;
    btree_init(&bt, 4);
    int keys[40];
    array_fill_random(keys, 40, 11u, 0, 200);
    for (int i = 0; i < 40; i++) {
      btree_insert(&bt, keys[i]);
    }
    int out[48];
    size_t n = btree_inorder(&bt, out, 48);
    ASSERT_EQ_INT(&t, (int)n, (int)btree_size(&bt));
    ASSERT_TRUE(&t, array_is_sorted_int(out, n));
    for (int i = 0; i < 40; i++) {
      btree_delete(&bt, keys[i]);
    }
    /* all keys that appeared should be gone (dups already collapsed) */
    for (int i = 0; i < 40; i++) {
      ASSERT_TRUE(&t, !btree_search(&bt, keys[i]));
    }
    ASSERT_EQ_INT(&t, (int)btree_size(&bt), 0);
    btree_destroy(&bt);
  }

  return test_report(&t, "btree");
}
