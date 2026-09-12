#include <stdio.h>
#include <stdlib.h>

#include "array.h"
#include "clrs.h"
#include "fib_heap.h"
#include "test.h"

int main(void) {
  TestSuite t;
  test_init(&t);

  /* empty insert/extract */
  {
    FibHeap h;
    fib_init(&h);
    fib_insert(&h, 10);
    ASSERT_EQ_INT(&t, fib_minimum(&h), 10);
    ASSERT_EQ_INT(&t, fib_size(&h), 1);
    ASSERT_EQ_INT(&t, fib_extract_min(&h), 10);
    ASSERT_EQ_INT(&t, (int)fib_size(&h), 0);
    fib_destroy(&h);
  }

  /* extract all in increasing order */
  {
    FibHeap h;
    fib_init(&h);
    int keys[] = {3, 1, 4, 1, 5, 9, 2, 6};
    for (int i = 0; i < 8; i++) {
      fib_insert(&h, keys[i]);
    }
    ASSERT_EQ_INT(&t, fib_minimum(&h), 1);
    int prev = -1000000;
    for (int i = 0; i < 8; i++) {
      int k = fib_extract_min(&h);
      if (k < prev) {
        fprintf(stderr, "  extract order broken: %d after %d\n", k, prev);
      }
      ASSERT_TRUE(&t, k >= prev);
      prev = k;
    }
    ASSERT_EQ_INT(&t, (int)fib_size(&h), 0);
    fib_destroy(&h);
  }

  /* larger random */
  {
    FibHeap h;
    fib_init(&h);
    int keys[50];
    array_fill_random(keys, 50, 7u, -20, 20);
    for (int i = 0; i < 50; i++) {
      fib_insert(&h, keys[i]);
    }
    ASSERT_EQ_INT(&t, (int)fib_size(&h), 50);
    int sorted[50];
    array_copy_int(sorted, keys, 50);
    qsort(sorted, 50, sizeof(int), clrs_cmp_int);
    for (int i = 0; i < 50; i++) {
      int k = fib_extract_min(&h);
      if (k != sorted[i]) {
        fprintf(stderr, "  expected %d got %d at %d\n", sorted[i], k, i);
      }
      ASSERT_EQ_INT(&t, k, sorted[i]);
    }
    fib_destroy(&h);
  }

  /* decrease-key */
  {
    FibHeap h;
    fib_init(&h);
    FibNode *a = fib_insert(&h, 10);
    fib_insert(&h, 20);
    FibNode *c = fib_insert(&h, 30);
    ASSERT_EQ_INT(&t, fib_minimum(&h), 10);
    fib_decrease_key(&h, c, 5);
    ASSERT_EQ_INT(&t, fib_minimum(&h), 5);
    fib_decrease_key(&h, a, 1);
    ASSERT_EQ_INT(&t, fib_minimum(&h), 1);
    ASSERT_EQ_INT(&t, fib_extract_min(&h), 1);
    ASSERT_EQ_INT(&t, fib_extract_min(&h), 5);
    ASSERT_EQ_INT(&t, fib_extract_min(&h), 20);
    fib_destroy(&h);
  }

  /* union */
  {
    FibHeap h1, h2;
    fib_init(&h1);
    fib_init(&h2);
    fib_insert(&h1, 3);
    fib_insert(&h1, 7);
    fib_insert(&h2, 1);
    fib_insert(&h2, 9);
    fib_union(&h1, &h2);
    ASSERT_EQ_INT(&t, (int)fib_size(&h1), 4);
    ASSERT_EQ_INT(&t, (int)fib_size(&h2), 0);
    ASSERT_EQ_INT(&t, fib_minimum(&h1), 1);
    ASSERT_EQ_INT(&t, fib_extract_min(&h1), 1);
    ASSERT_EQ_INT(&t, fib_extract_min(&h1), 3);
    ASSERT_EQ_INT(&t, fib_extract_min(&h1), 7);
    ASSERT_EQ_INT(&t, fib_extract_min(&h1), 9);
    fib_destroy(&h1);
    fib_destroy(&h2);
  }

  /* regression: decrease-key on the child of a promoted (marked) root used
   * to dereference NULL in cascading-cut. Exact crash sequence. */
  {
    FibHeap h;
    fib_init(&h);
    fib_insert(&h, 1);
    fib_insert(&h, 4);
    FibNode *n5 = fib_insert(&h, 5);
    FibNode *n6 = fib_insert(&h, 6);
    fib_insert(&h, 8);
    ASSERT_EQ_INT(&t, fib_extract_min(&h), 1); /* build 4(children 5,6(->8)) */
    fib_insert(&h, 3);
    ASSERT_EQ_INT(&t, fib_extract_min(&h), 3);
    fib_insert(&h, 2); /* roots [4, 2] */
    fib_insert(&h, 0);
    ASSERT_EQ_INT(&t, fib_extract_min(&h), 0); /* consolidate: 2 -> child 4 */
    fib_decrease_key(&h, n5, 0); /* cut(5,4): 4 becomes marked non-root */
    ASSERT_EQ_INT(&t, fib_extract_min(&h), 0);
    ASSERT_EQ_INT(&t, fib_extract_min(&h), 2); /* promotes marked root 4 */
    fib_decrease_key(&h, n6, 3); /* cut(6,4): cascading-cut hits root 4 */
    ASSERT_EQ_INT(&t, fib_minimum(&h), 3);
    ASSERT_EQ_INT(&t, fib_extract_min(&h), 3);
    ASSERT_EQ_INT(&t, fib_extract_min(&h), 4);
    ASSERT_EQ_INT(&t, fib_extract_min(&h), 8);
    ASSERT_EQ_INT(&t, (int)fib_size(&h), 0);
    fib_destroy(&h);
  }

  /* randomized insert/decrease-key/extract cross-checked with a reference */
  {
    FibHeap h;
    fib_init(&h);
    enum { MAXN = 300, OPS = 3000 };
    FibNode *nodes[MAXN];
    int keys[MAXN];
    int live[MAXN]; /* live[i] = index of i-th live entry */
    size_t nlive = 0;
    size_t nins = 0;
    srand(99u);
    int collected[2 * MAXN];
    int refsorted[2 * MAXN];
    for (int op = 0; op < OPS; op++) {
      int r = clrs_rand_range_int(0, 2);
      if (nlive == 0 || (r == 0 && nins < MAXN)) {
        if (nins < MAXN) {
          int k = clrs_rand_range_int(-5000, 5000);
          nodes[nins] = fib_insert(&h, k);
          keys[nins] = k;
          live[nlive++] = (int)nins;
          nins++;
        }
      } else if (r == 1) {
        size_t li = (size_t)clrs_rand_range_int(0, (int)nlive - 1);
        int i = live[li];
        keys[i] -= clrs_rand_range_int(1, 100);
        fib_decrease_key(&h, nodes[i], keys[i]);
      } else {
        size_t min_li = 0;
        for (size_t li = 1; li < nlive; li++) {
          if (keys[live[li]] < keys[live[min_li]]) {
            min_li = li;
          }
        }
        int want = keys[live[min_li]];
        int got = fib_extract_min(&h);
        ASSERT_EQ_INT(&t, got, want);
        live[min_li] = live[nlive - 1];
        nlive--;
      }
      ASSERT_EQ_INT(&t, (int)fib_size(&h), (int)nlive);
      if (op % 250 == 0 || nlive == 0) {
        size_t got = fib_collect(&h, collected, 2 * MAXN);
        ASSERT_EQ_INT(&t, (int)got, (int)nlive);
        for (size_t i = 0; i < nlive; i++) {
          refsorted[i] = keys[live[i]];
        }
        qsort(refsorted, nlive, sizeof(int), clrs_cmp_int);
        qsort(collected, got, sizeof(int), clrs_cmp_int);
        for (size_t i = 0; i < nlive; i++) {
          ASSERT_EQ_INT(&t, collected[i], refsorted[i]);
        }
      }
    }
    while (nlive > 0) {
      size_t min_li = 0;
      for (size_t li = 1; li < nlive; li++) {
        if (keys[live[li]] < keys[live[min_li]]) {
          min_li = li;
        }
      }
      ASSERT_EQ_INT(&t, fib_extract_min(&h), keys[live[min_li]]);
      live[min_li] = live[nlive - 1];
      nlive--;
    }
    ASSERT_EQ_INT(&t, (int)fib_size(&h), 0);
    fib_destroy(&h);
  }

  return test_report(&t, "fib_heap");
}
