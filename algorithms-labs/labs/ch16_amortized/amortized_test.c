#include <stdio.h>
#include <stdlib.h>

#include "clrs.h"
#include "dynamic_table.h"
#include "test.h"

int main(void) {
  TestSuite t;
  test_init(&t);

  /* basic push/size growth */
  {
    DynTable dt;
    dtab_init(&dt);
    ASSERT_EQ_INT(&t, (int)dt.size, 0);
    dtab_push(&dt, 1);
    ASSERT_EQ_INT(&t, (int)dt.n, 1);
    ASSERT_EQ_INT(&t, (int)dt.size, 1);
    dtab_push(&dt, 2);
    ASSERT_EQ_INT(&t, (int)dt.size, 2);
    dtab_push(&dt, 3);
    ASSERT_EQ_INT(&t, (int)dt.size, 4);
    ASSERT_EQ_INT(&t, dtab_at(&dt, 0), 1);
    ASSERT_EQ_INT(&t, dtab_at(&dt, 2), 3);
    dtab_destroy(&dt);
  }

  /* powers of two sizes after 1,2,4,8,... inserts */
  {
    DynTable dt;
    dtab_init(&dt);
    for (int i = 1; i <= 16; i++) {
      dtab_push(&dt, i);
    }
    ASSERT_EQ_INT(&t, (int)dt.n, 16);
    ASSERT_EQ_INT(&t, (int)dt.size, 16);
    for (int i = 0; i < 16; i++) {
      ASSERT_EQ_INT(&t, dtab_at(&dt, (size_t)i), i + 1);
    }
    dtab_destroy(&dt);
  }

  /* contraction: fill 8, pop down to 1 — size should shrink */
  {
    DynTable dt;
    dtab_init(&dt);
    for (int i = 0; i < 8; i++) {
      dtab_push(&dt, i);
    }
    ASSERT_EQ_INT(&t, (int)dt.size, 8);
    /* pop until n < size/4 => n < 2, so n=1 triggers after pop from 2 */
    int x = dtab_pop(&dt);
    ASSERT_EQ_INT(&t, x, 7);
    dtab_pop(&dt); /* 6 */
    dtab_pop(&dt); /* 5 */
    dtab_pop(&dt); /* 4  n=4 size=8  4*4=16 > 8 no shrink?  n*4 <= size => 16 <= 8 false */
    ASSERT_EQ_INT(&t, (int)dt.n, 4);
    dtab_pop(&dt); /* 3  n=3  12 <= 8? no */
    dtab_pop(&dt); /* 2  n=2  8 <= 8 yes → size 4 */
    ASSERT_EQ_INT(&t, (int)dt.n, 2);
    ASSERT_EQ_INT(&t, (int)dt.size, 4);
    dtab_pop(&dt); /* 1  n=1  4 <= 4 yes → size 2 */
    ASSERT_EQ_INT(&t, (int)dt.n, 1);
    ASSERT_EQ_INT(&t, (int)dt.size, 2);
    ASSERT_EQ_INT(&t, dtab_at(&dt, 0), 0);
    dtab_destroy(&dt);
  }

  /* amortized cost: n inserts cause O(n) total element moves */
  {
    DynTable dt;
    dtab_init(&dt);
    const int N = 1024;
    for (int i = 0; i < N; i++) {
      dtab_push(&dt, i);
    }
    /* Total moves on expansion: 1+2+4+...+512 = N-1 */
    ASSERT_EQ_INT(&t, (int)dt.resizes, N - 1);
    ASSERT_EQ_INT(&t, (int)dt.size, N);
    dtab_destroy(&dt);
  }

  /* mixed insert/delete keeps n and content consistent */
  {
    DynTable dt;
    dtab_init(&dt);
    for (int i = 0; i < 20; i++) {
      dtab_push(&dt, i * 2);
    }
    for (int i = 0; i < 10; i++) {
      dtab_pop(&dt);
    }
    ASSERT_EQ_INT(&t, (int)dt.n, 10);
    ASSERT_EQ_INT(&t, dtab_at(&dt, 0), 0);
    ASSERT_EQ_INT(&t, dtab_at(&dt, 9), 18);
    dtab_destroy(&dt);
  }

  return test_report(&t, "amortized");
}
