#include <stdio.h>
#include <stdlib.h>

#include "clrs.h"
#include "test.h"
#include "veb.h"

int main(void) {
  TestSuite t;
  test_init(&t);

  /* empty */
  {
    VEB *v = veb_create(16);
    ASSERT_EQ_INT(&t, veb_minimum(v), VEB_NIL);
    ASSERT_EQ_INT(&t, veb_maximum(v), VEB_NIL);
    ASSERT_EQ_INT(&t, veb_member(v, 5), 0);
    ASSERT_EQ_INT(&t, veb_successor(v, 0), VEB_NIL);
    ASSERT_EQ_INT(&t, veb_predecessor(v, 15), VEB_NIL);
    ASSERT_EQ_INT(&t, (int)veb_size(v), 0);
    veb_destroy(v);
  }

  /* insert/member/min/max */
  {
    VEB *v = veb_create(16);
    veb_insert(v, 3);
    veb_insert(v, 5);
    veb_insert(v, 9);
    ASSERT_EQ_INT(&t, veb_member(v, 3), 1);
    ASSERT_EQ_INT(&t, veb_member(v, 4), 0);
    ASSERT_EQ_INT(&t, veb_minimum(v), 3);
    ASSERT_EQ_INT(&t, veb_maximum(v), 9);
    ASSERT_EQ_INT(&t, (int)veb_size(v), 3);
    veb_destroy(v);
  }

  /* successor / predecessor */
  {
    VEB *v = veb_create(16);
    int keys[] = {1, 3, 4, 7, 15};
    for (int i = 0; i < 5; i++) {
      veb_insert(v, keys[i]);
    }
    ASSERT_EQ_INT(&t, veb_successor(v, 0), 1);
    ASSERT_EQ_INT(&t, veb_successor(v, 1), 3);
    ASSERT_EQ_INT(&t, veb_successor(v, 4), 7);
    ASSERT_EQ_INT(&t, veb_successor(v, 7), 15);
    ASSERT_EQ_INT(&t, veb_successor(v, 15), VEB_NIL);
    ASSERT_EQ_INT(&t, veb_predecessor(v, 15), 7);
    ASSERT_EQ_INT(&t, veb_predecessor(v, 7), 4);
    ASSERT_EQ_INT(&t, veb_predecessor(v, 1), VEB_NIL);
    ASSERT_EQ_INT(&t, veb_predecessor(v, 0), VEB_NIL);
    veb_destroy(v);
  }

  /* universe 2 */
  {
    VEB *v = veb_create(2);
    veb_insert(v, 0);
    ASSERT_EQ_INT(&t, veb_successor(v, 0), VEB_NIL); /* only {0} */
    ASSERT_EQ_INT(&t, veb_member(v, 1), 0);
    veb_insert(v, 1);
    ASSERT_EQ_INT(&t, veb_member(v, 1), 1);
    ASSERT_EQ_INT(&t, veb_predecessor(v, 1), 0);
    ASSERT_EQ_INT(&t, veb_successor(v, 0), 1);
    veb_delete(v, 0);
    ASSERT_EQ_INT(&t, veb_member(v, 0), 0);
    ASSERT_EQ_INT(&t, veb_minimum(v), 1);
    ASSERT_EQ_INT(&t, veb_maximum(v), 1);
    ASSERT_EQ_INT(&t, veb_successor(v, 0), 1);
    veb_destroy(v);
  }

  /* delete min / max / interior */
  {
    VEB *v = veb_create(16);
    for (int i = 0; i < 8; i++) {
      veb_insert(v, i * 2); /* 0,2,4,6,8,10,12,14 */
    }
    ASSERT_EQ_INT(&t, (int)veb_size(v), 8);
    ASSERT_EQ_INT(&t, veb_delete(v, 0), 1);
    ASSERT_EQ_INT(&t, veb_minimum(v), 2);
    ASSERT_EQ_INT(&t, veb_delete(v, 14), 1);
    ASSERT_EQ_INT(&t, veb_maximum(v), 12);
    ASSERT_EQ_INT(&t, veb_delete(v, 6), 1);
    ASSERT_EQ_INT(&t, veb_member(v, 6), 0);
    ASSERT_EQ_INT(&t, veb_successor(v, 4), 8);
    ASSERT_EQ_INT(&t, veb_predecessor(v, 8), 4);
    ASSERT_EQ_INT(&t, veb_delete(v, 1), 0); /* absent */
    ASSERT_EQ_INT(&t, (int)veb_size(v), 5);
    veb_destroy(v);
  }

  /* full small universe */
  {
    VEB *v = veb_create(8);
    for (int i = 0; i < 8; i++) {
      veb_insert(v, i);
    }
    ASSERT_EQ_INT(&t, veb_minimum(v), 0);
    ASSERT_EQ_INT(&t, veb_maximum(v), 7);
    for (int i = 0; i < 8; i++) {
      ASSERT_EQ_INT(&t, veb_member(v, i), 1);
    }
    for (int i = 0; i < 8; i++) {
      ASSERT_EQ_INT(&t, veb_delete(v, i), 1);
    }
    ASSERT_EQ_INT(&t, veb_minimum(v), VEB_NIL);
    ASSERT_EQ_INT(&t, veb_maximum(v), VEB_NIL);
    veb_destroy(v);
  }

  /* larger universe 64 */
  {
    VEB *v = veb_create(64);
    for (int i = 0; i < 64; i += 3) {
      veb_insert(v, i);
    }
    ASSERT_EQ_INT(&t, veb_minimum(v), 0);
    ASSERT_EQ_INT(&t, veb_successor(v, 0), 3);
    ASSERT_EQ_INT(&t, veb_successor(v, 30), 33);
    ASSERT_EQ_INT(&t, veb_predecessor(v, 33), 30);
    ASSERT_EQ_INT(&t, veb_maximum(v), 63);
    veb_destroy(v);
  }

  /* duplicate inserts are ignored (first wins) */
  {
    VEB *v = veb_create(16);
    veb_insert(v, 5);
    veb_insert(v, 5);
    veb_insert(v, 5);
    ASSERT_EQ_INT(&t, (int)veb_size(v), 1);
    ASSERT_EQ_INT(&t, veb_member(v, 5), 1);
    ASSERT_EQ_INT(&t, veb_minimum(v), 5);
    ASSERT_EQ_INT(&t, veb_maximum(v), 5);
    ASSERT_EQ_INT(&t, veb_successor(v, 4), 5);
    ASSERT_EQ_INT(&t, veb_successor(v, 5), VEB_NIL);
    ASSERT_EQ_INT(&t, veb_delete(v, 5), 1);
    ASSERT_EQ_INT(&t, (int)veb_size(v), 0);
    ASSERT_EQ_INT(&t, veb_member(v, 5), 0); /* no ghost after delete */
    veb_insert(v, 7);
    ASSERT_EQ_INT(&t, veb_member(v, 5), 0);
    ASSERT_EQ_INT(&t, veb_member(v, 7), 1);
    veb_destroy(v);
  }

  /* out-of-range queries are safe */
  {
    VEB *v = veb_create(16);
    ASSERT_EQ_INT(&t, veb_member(v, -1), 0);
    ASSERT_EQ_INT(&t, veb_member(v, -4), 0);
    ASSERT_EQ_INT(&t, veb_member(v, 16), 0);
    ASSERT_EQ_INT(&t, veb_successor(v, -1), VEB_NIL);
    ASSERT_EQ_INT(&t, veb_predecessor(v, 16), VEB_NIL);
    veb_destroy(v);
  }

  /* random ops cross-checked against a bitmap reference */
  {
    enum { U = 256, OPS = 4000 };
    VEB *v = veb_create(U);
    unsigned char ref[U];
    for (int i = 0; i < U; i++) {
      ref[i] = 0;
    }
    srand(20260912u);
    size_t live = 0;
    for (int op = 0; op < OPS; op++) {
      int x = clrs_rand_range_int(0, U - 1);
      int kind = clrs_rand_range_int(0, 4);
      if (kind <= 1) { /* insert */
        veb_insert(v, x);
        if (!ref[x]) {
          ref[x] = 1;
          live++;
        }
      } else if (kind == 2) { /* delete */
        ASSERT_EQ_INT(&t, veb_delete(v, x), ref[x]);
        if (ref[x]) {
          ref[x] = 0;
          live--;
        }
      } else if (kind == 3) { /* member + successor */
        if (x + 1 < U) {
          int exp = VEB_NIL;
          for (int y = x + 1; y < U; y++) {
            if (ref[y]) {
              exp = y;
              break;
            }
          }
          ASSERT_EQ_INT(&t, veb_successor(v, x), exp);
        }
        ASSERT_EQ_INT(&t, veb_member(v, x), ref[x]);
      } else { /* predecessor */
        int exp = VEB_NIL;
        for (int y = x - 1; y >= 0; y--) {
          if (ref[y]) {
            exp = y;
            break;
          }
        }
        ASSERT_EQ_INT(&t, veb_predecessor(v, x), exp);
      }
      if (op % 500 == 0) {
        ASSERT_EQ_INT(&t, (int)veb_size(v), (int)live);
      }
    }
    /* final full consistency sweep */
    ASSERT_EQ_INT(&t, (int)veb_size(v), (int)live);
    int expect_min = VEB_NIL, expect_max = VEB_NIL;
    for (int i = 0; i < U; i++) {
      if (ref[i]) {
        if (expect_min == VEB_NIL) {
          expect_min = i;
        }
        expect_max = i;
      }
      ASSERT_EQ_INT(&t, veb_member(v, i), ref[i]);
    }
    ASSERT_EQ_INT(&t, veb_minimum(v), expect_min);
    ASSERT_EQ_INT(&t, veb_maximum(v), expect_max);
    veb_destroy(v);
  }

  return test_report(&t, "veb");
}
