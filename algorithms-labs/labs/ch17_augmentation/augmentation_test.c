#include <stdio.h>
#include <stdlib.h>

#include "clrs.h"
#include "interval_tree.h"
#include "order_stat_tree.h"
#include "test.h"

int main(void) {
  TestSuite t;
  test_init(&t);

  /* --- order statistic tree --- */
  {
    OSTree st;
    ost_init(&st);
    int keys[] = {20, 10, 30, 5, 15, 25, 35};
    for (int i = 0; i < 7; i++) {
      ost_insert(&st, keys[i]);
      ASSERT_TRUE(&t, ost_validate(&st));
    }
    ASSERT_EQ_INT(&t, (int)ost_size(&st), 7);

    /* select 0..6 sorted: 5,10,15,20,25,30,35 */
    int expect[] = {5, 10, 15, 20, 25, 30, 35};
    for (size_t i = 0; i < 7; i++) {
      OSTNode *x = ost_select(&st, i);
      ASSERT_TRUE(&t, x != NULL);
      ASSERT_EQ_INT(&t, x->key, expect[i]);
    }
    ASSERT_TRUE(&t, ost_select(&st, 7) == NULL);

    for (size_t i = 0; i < 7; i++) {
      ASSERT_EQ_INT(&t, (int)ost_rank(&st, expect[i]), (int)i);
    }

    ASSERT_TRUE(&t, ost_delete(&st, 20));
    ASSERT_TRUE(&t, ost_validate(&st));
    ASSERT_EQ_INT(&t, (int)ost_size(&st), 6);
    ASSERT_TRUE(&t, ost_select(&st, 3)->key == 25); /* 5,10,15,25,... */
    ASSERT_EQ_INT(&t, (int)ost_rank(&st, 25), 3);

    ost_destroy(&st);
  }

  /* stress: sequential + reverse insert/delete, keep validating */
  {
    OSTree st;
    ost_init(&st);
    for (int i = 1; i <= 32; i++) {
      ost_insert(&st, i);
      ASSERT_TRUE(&t, ost_validate(&st));
    }
    for (int i = 1; i <= 32; i += 2) {
      ASSERT_TRUE(&t, ost_delete(&st, i));
      ASSERT_TRUE(&t, ost_validate(&st));
    }
    ASSERT_EQ_INT(&t, (int)ost_size(&st), 16);
    ASSERT_EQ_INT(&t, ost_select(&st, 0)->key, 2);
    ASSERT_EQ_INT(&t, ost_select(&st, 15)->key, 32);
    for (int i = 2; i <= 32; i += 2) {
      ASSERT_EQ_INT(&t, (int)ost_rank(&st, i), (i - 2) / 2);
    }
    ost_destroy(&st);
  }

  {
    OSTree st;
    ost_init(&st);
    ost_insert(&st, 42);
    ASSERT_EQ_INT(&t, (int)ost_select(&st, 0)->key, 42);
    ASSERT_EQ_INT(&t, (int)ost_rank(&st, 42), 0);
    ost_destroy(&st);
  }

  /* randomized insert/delete cross-checked with a sorted reference:
   * exercises select/rank/size after every op, in particular the
   * path-based size maintenance on delete */
  {
    OSTree st;
    ost_init(&st);
    enum { CAP = 256 };
    int ref[CAP];
    size_t nn = 0;
    srand(17u);
    for (int op = 0; op < 1200; op++) {
      int k = clrs_rand_range_int(0, 300);
      int kind = clrs_rand_range_int(0, 2);
      if (kind <= 1) {
        /* reference insert (unique keys) */
        if (nn < CAP) {
          size_t lo = 0;
          while (lo < nn && ref[lo] < k) {
            lo++;
          }
          if (lo == nn || ref[lo] != k) {
            for (size_t i = nn; i > lo; i--) {
              ref[i] = ref[i - 1];
            }
            ref[lo] = k;
            nn++;
            ASSERT_TRUE(&t, ost_insert(&st, k) != NULL);
          }
        }
      } else {
        size_t lo = 0;
        while (lo < nn && ref[lo] < k) {
          lo++;
        }
        int had = (lo < nn && ref[lo] == k);
        ASSERT_EQ_INT(&t, ost_delete(&st, k), had);
        if (had) {
          for (size_t i = lo; i + 1 < nn; i++) {
            ref[i] = ref[i + 1];
          }
          nn--;
        }
      }
      ASSERT_TRUE(&t, ost_validate(&st));
      ASSERT_EQ_INT(&t, (int)ost_size(&st), (int)nn);
      for (size_t i = 0; i < nn; i++) {
        ASSERT_EQ_INT(&t, ost_select(&st, i)->key, ref[i]);
        ASSERT_EQ_INT(&t, (int)ost_rank(&st, ref[i]), (int)i);
      }
    }
    ost_destroy(&st);
  }

  /* --- interval tree --- */
  {
    /* CLRS Fig 14.3-ish intervals */
    ITree it;
    itree_init(&it);
    itree_insert(&it, 17, 19);
    itree_insert(&it, 5, 11);
    itree_insert(&it, 4, 8);
    itree_insert(&it, 15, 18);
    itree_insert(&it, 7, 10);
    itree_insert(&it, 16, 22);
    itree_insert(&it, 21, 23);

    /* query [14,16] overlaps 15-18, 16-22 */
    ITNode *x = itree_search_overlap(&it, 14, 16);
    ASSERT_TRUE(&t, x != NULL);
    ASSERT_TRUE(&t, x->iv.low <= 16 && x->iv.high >= 14);

    size_t n = itree_count_overlaps(&it, 14, 16);
    ASSERT_EQ_INT(&t, (int)n, 2);

    ASSERT_EQ_INT(&t, (int)itree_count_overlaps(&it, 12, 13), 0);
    ASSERT_TRUE(&t, itree_search_overlap(&it, 12, 13) == NULL);

    /* all intervals overlap [0, 100] */
    ASSERT_EQ_INT(&t, (int)itree_count_overlaps(&it, 0, 100), 7);

    ASSERT_TRUE(&t, itree_delete(&it, 16));
    ASSERT_EQ_INT(&t, (int)itree_count_overlaps(&it, 14, 16), 1);
    itree_destroy(&it);
  }

  {
    ITree it;
    itree_init(&it);
    itree_insert(&it, 1, 2);
    itree_insert(&it, 3, 4);
    itree_insert(&it, 5, 6);
    ASSERT_TRUE(&t, itree_search_overlap(&it, 3, 3) != NULL);
    ASSERT_TRUE(&t, itree_search_overlap(&it, 7, 8) == NULL);
    itree_destroy(&it);
  }

  return test_report(&t, "augmentation");
}
