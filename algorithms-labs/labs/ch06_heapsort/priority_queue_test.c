#include <stdio.h>

#include "clrs.h"
#include "priority_queue.h"
#include "test.h"

int main(void) {
  TestSuite t;
  test_init(&t);

  MaxPriorityQueue q;
  max_pq_init(&q, 16);

  ASSERT_TRUE(&t, q.heap_size == 0);

  max_pq_insert(&q, 3);
  max_pq_insert(&q, 1);
  max_pq_insert(&q, 17);
  max_pq_insert(&q, 5);
  max_pq_insert(&q, 9);
  max_pq_insert(&q, 12);

  ASSERT_EQ_INT(&t, (int)q.heap_size, 6);
  ASSERT_EQ_INT(&t, max_pq_maximum(&q), 17);

  ASSERT_EQ_INT(&t, max_pq_extract_max(&q), 17);
  ASSERT_EQ_INT(&t, max_pq_maximum(&q), 12);

  max_pq_insert(&q, 20);
  ASSERT_EQ_INT(&t, max_pq_maximum(&q), 20);

  /* Extract in descending order */
  int prev = max_pq_extract_max(&q);
  int ordered = 1;
  while (q.heap_size > 0) {
    int cur = max_pq_extract_max(&q);
    if (cur > prev) {
      ordered = 0;
    }
    prev = cur;
  }
  ASSERT_TRUE(&t, ordered);

  /* increase-key path (release the previous buffer first) */
  max_pq_destroy(&q);
  max_pq_init(&q, 8);
  max_pq_insert(&q, 1);
  max_pq_insert(&q, 2);
  max_pq_insert(&q, 3);
  max_pq_increase_key(&q, 2, 100); /* index 2 is some leaf */
  ASSERT_EQ_INT(&t, max_pq_maximum(&q), 100);

  max_pq_destroy(&q);
  return test_report(&t, "priority_queue");
}
