#include <stdio.h>
#include <stdlib.h>

#include "clrs.h"
#include "disjoint_set.h"
#include "test.h"

int main(void) {
  TestSuite t;
  test_init(&t);

  /* make-set / find / same */
  {
    DisjointSet s;
    ds_init(&s, 5);
    for (size_t i = 0; i < 5; i++) {
      ASSERT_EQ_INT(&t, ds_find(&s, i), (int)i);
      ASSERT_EQ_INT(&t, ds_find(&s, i), (int)i);
    }
    ASSERT_TRUE(&t, !ds_connected(&s, 0, 1));
    ds_destroy(&s);
  }

  /* union and connectivity */
  {
    DisjointSet s;
    ds_init(&s, 10);
    ASSERT_TRUE(&t, ds_union(&s, 0, 1));
    ASSERT_TRUE(&t, ds_union(&s, 1, 2));
    ASSERT_TRUE(&t, ds_union(&s, 3, 4));
    ASSERT_TRUE(&t, !ds_union(&s, 0, 2)); /* already same set */
    ASSERT_TRUE(&t, ds_connected(&s, 0, 2));
    ASSERT_TRUE(&t, ds_connected(&s, 1, 2));
    ASSERT_TRUE(&t, !ds_connected(&s, 0, 3));
    ds_union(&s, 2, 4);
    ASSERT_TRUE(&t, ds_connected(&s, 0, 4));
    ASSERT_TRUE(&t, ds_connected(&s, 1, 3));
    ASSERT_EQ_INT(&t, ds_find(&s, 0), ds_find(&s, 4));
    ds_destroy(&s);
  }

  /* textbook Fig 21.1 / 21.2 style sequence */
  {
    DisjointSet s;
    ds_init(&s, 8); /* 0..7 as 1..8 minus 1 */
    ds_union(&s, 0, 1); /* 1-2 */
    ds_union(&s, 2, 3);
    ds_union(&s, 3, 4);
    ds_union(&s, 5, 6);
    ds_union(&s, 6, 7);
    ds_union(&s, 1, 3); /* {0,1,2,3,4} */
    ds_union(&s, 4, 7); /* all one set */
    for (size_t i = 0; i < 8; i++) {
      ASSERT_TRUE(&t, ds_connected(&s, 0, i));
    }
    ds_destroy(&s);
  }

  /* connected components */
  {
    /* edges: 0-1, 1-2, 3-4, 5 */
    int eu[] = {0, 1, 3};
    int ev[] = {1, 2, 4};
    int comp[6];
    size_t k = ds_connected_components(6, eu, ev, 3, comp);
    ASSERT_EQ_INT(&t, (int)k, 3);
    ASSERT_EQ_INT(&t, comp[0], comp[2]);
    ASSERT_EQ_INT(&t, comp[3], comp[4]);
    ASSERT_TRUE(&t, comp[0] != comp[3]);
    ASSERT_TRUE(&t, comp[5] != comp[0] && comp[5] != comp[3]);
  }

  /* no edges: n components */
  {
    int comp[4];
    size_t k = ds_connected_components(4, NULL, NULL, 0, comp);
    ASSERT_EQ_INT(&t, (int)k, 4);
    ASSERT_TRUE(&t, comp[0] != comp[1]);
  }

  /* star graph: one component */
  {
    int eu[] = {0, 0, 0, 0};
    int ev[] = {1, 2, 3, 4};
    int comp[5];
    size_t k = ds_connected_components(5, eu, ev, 4, comp);
    ASSERT_EQ_INT(&t, (int)k, 1);
  }

  /* many unions stay consistent */
  {
    DisjointSet s;
    ds_init(&s, 100);
    for (int i = 0; i < 99; i++) {
      ds_union(&s, (size_t)i, (size_t)i + 1);
    }
    for (size_t i = 0; i < 100; i++) {
      ASSERT_TRUE(&t, ds_connected(&s, 0, i));
    }
    ASSERT_EQ_INT(&t, ds_find(&s, 0), ds_find(&s, 99));
    ds_destroy(&s);
  }

  return test_report(&t, "disjoint_sets");
}
