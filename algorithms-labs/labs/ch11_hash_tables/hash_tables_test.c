#include <stdio.h>
#include <stdlib.h>

#include "clrs.h"
#include "direct_address.h"
#include "hash_chain.h"
#include "hash_open_address.h"
#include "test.h"

int main(void) {
  TestSuite t;
  test_init(&t);

  /* --- direct address --- */
  {
    DirectAddress d;
    dad_init(&d, 16);
    dad_insert(&d, 3, 30);
    dad_insert(&d, 7, 70);
    ASSERT_EQ_INT(&t, dad_search(&d, 3), 30);
    ASSERT_EQ_INT(&t, dad_search(&d, 7), 70);
    ASSERT_EQ_INT(&t, dad_search(&d, 5), 0);
    dad_delete(&d, 3);
    ASSERT_EQ_INT(&t, dad_search(&d, 3), 0);
    dad_destroy(&d);
  }

  /* --- hash functions --- */
  {
    ASSERT_EQ_INT(&t, (int)hash_division(10, 7), 3);
    ASSERT_EQ_INT(&t, (int)hash_division(14, 7), 0);
    ASSERT_TRUE(&t, hash_multiplication(12345, 16) < 16);
  }

  /* --- chained hash --- */
  {
    ChainedHash ht;
    chained_hash_init(&ht, 11);
    for (int i = 0; i < 20; i++) {
      chained_hash_insert(&ht, i * 3, i * 100);
    }
    int v = 0;
    ASSERT_TRUE(&t, chained_hash_search(&ht, 0, &v));
    ASSERT_EQ_INT(&t, v, 0);
    ASSERT_TRUE(&t, chained_hash_search(&ht, 33, &v)); /* 33 = 3*11 */
    ASSERT_EQ_INT(&t, v, 1100);
    ASSERT_TRUE(&t, !chained_hash_search(&ht, 4, &v));
    ASSERT_TRUE(&t, chained_hash_delete(&ht, 33));
    ASSERT_TRUE(&t, !chained_hash_search(&ht, 33, &v));
    ASSERT_TRUE(&t, !chained_hash_delete(&ht, 33));
    chained_hash_destroy(&ht);
  }

  {
    /* same slot with m=3: keys 0,3,6 */
    ChainedHash ht;
    chained_hash_init(&ht, 3);
    chained_hash_insert(&ht, 0, 1);
    chained_hash_insert(&ht, 3, 2);
    chained_hash_insert(&ht, 6, 3);
    int v;
    ASSERT_TRUE(&t, chained_hash_search(&ht, 0, &v));
    ASSERT_EQ_INT(&t, v, 1);
    ASSERT_TRUE(&t, chained_hash_search(&ht, 3, &v));
    ASSERT_EQ_INT(&t, v, 2);
    ASSERT_TRUE(&t, chained_hash_search(&ht, 6, &v));
    ASSERT_EQ_INT(&t, v, 3);
    ASSERT_TRUE(&t, chained_hash_delete(&ht, 3));
    ASSERT_TRUE(&t, chained_hash_search(&ht, 0, &v));
    ASSERT_TRUE(&t, chained_hash_search(&ht, 6, &v));
    ASSERT_TRUE(&t, !chained_hash_search(&ht, 3, &v));
    chained_hash_destroy(&ht);
  }

  /* --- open addressing linear probe --- */
  {
    OpenHash oh;
    open_hash_init(&oh, 17);
    for (int i = 0; i < 10; i++) {
      open_hash_insert(&oh, i * 2 + 1, 100 + i);
    }
    int v = 0;
    ASSERT_TRUE(&t, open_hash_search(&oh, 1, &v));
    ASSERT_EQ_INT(&t, v, 100);
    ASSERT_TRUE(&t, open_hash_search(&oh, 19, &v));
    ASSERT_EQ_INT(&t, v, 109);
    ASSERT_TRUE(&t, !open_hash_search(&oh, 2, &v));
    open_hash_destroy(&oh);
  }

  {
    /* linear probe: 5 and 22 collide when m=17 */
    OpenHash oh;
    int v = 0;
    open_hash_init(&oh, 17);
    open_hash_insert(&oh, 5, 50);
    open_hash_insert(&oh, 22, 220);
    ASSERT_TRUE(&t, open_hash_search(&oh, 5, &v));
    ASSERT_EQ_INT(&t, v, 50);
    ASSERT_TRUE(&t, open_hash_search(&oh, 22, &v));
    ASSERT_EQ_INT(&t, v, 220);
    ASSERT_TRUE(&t, open_hash_delete(&oh, 5));
    ASSERT_TRUE(&t, open_hash_search(&oh, 22, &v));
    ASSERT_EQ_INT(&t, v, 220);
    ASSERT_TRUE(&t, !open_hash_search(&oh, 5, &v));
    open_hash_destroy(&oh);
  }

  {
    /* regression: re-inserting a key whose probe chain crosses a
     * tombstone must update the original entry, not duplicate it.
     * m=7: 0->slot 0, 7->slot 1, 14->slot 2. Delete 0 (tombstone at 0),
     * re-insert 14: the tombstone sits earlier on 14's chain. */
    OpenHash oh;
    int v = 0;
    open_hash_init(&oh, 7);
    open_hash_insert(&oh, 0, 1);
    open_hash_insert(&oh, 7, 2);
    open_hash_insert(&oh, 14, 3);
    ASSERT_TRUE(&t, open_hash_delete(&oh, 0));
    open_hash_insert(&oh, 14, 33); /* must update slot 2, not reuse slot 0 */
    ASSERT_EQ_INT(&t, (int)oh.n, 2);
    ASSERT_TRUE(&t, open_hash_search(&oh, 14, &v));
    ASSERT_EQ_INT(&t, v, 33);
    ASSERT_TRUE(&t, open_hash_delete(&oh, 14));
    ASSERT_TRUE(&t, !open_hash_search(&oh, 14, &v)); /* no ghost copy */
    ASSERT_EQ_INT(&t, (int)oh.n, 1); /* only key 7 remains */
    open_hash_destroy(&oh);
  }

  {
    /* randomized upsert/delete against a reference array (m small to
     * force collisions and tombstones) */
    OpenHash oh;
    open_hash_init(&oh, 11);
    enum { CAP = 64 };
    int rk[CAP], rv[CAP];
    size_t nn = 0;
    srand(11u);
    for (int op = 0; op < 600; op++) {
      int k = clrs_rand_range_int(0, 40);
      int kind = clrs_rand_range_int(0, 2);
      if (kind <= 1) {
        int val = clrs_rand_range_int(0, 1000);
        /* reference upsert; a brand-new key must still fit the table */
        size_t at = nn;
        for (size_t i = 0; i < nn; i++) {
          if (rk[i] == k) {
            at = i;
            break;
          }
        }
        if (at == nn && nn == 11) {
          continue; /* table full, key absent: skip this op */
        }
        if (at == nn) {
          ASSERT_TRUE(&t, nn < CAP);
          rk[at] = k;
          nn++;
        }
        rv[at] = val;
        open_hash_insert(&oh, k, val);
      } else {
        int had = 0;
        for (size_t i = 0; i < nn; i++) {
          if (rk[i] == k) {
            had = 1;
          }
        }
        ASSERT_EQ_INT(&t, open_hash_delete(&oh, k), had);
        if (had) {
          size_t w = 0;
          for (size_t i = 0; i < nn; i++) {
            if (rk[i] != k) {
              rk[w] = rk[i];
              rv[w] = rv[i];
              w++;
            }
          }
          nn = w;
        }
      }
      ASSERT_EQ_INT(&t, (int)oh.n, (int)nn);
      for (size_t i = 0; i < nn; i++) {
        int v = 0;
        ASSERT_TRUE(&t, open_hash_search(&oh, rk[i], &v));
        ASSERT_EQ_INT(&t, v, rv[i]);
      }
    }
    open_hash_destroy(&oh);
  }

  return test_report(&t, "hash_tables");
}
