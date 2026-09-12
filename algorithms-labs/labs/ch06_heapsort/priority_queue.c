#include "priority_queue.h"

#include <stdlib.h>

#include "clrs.h"
#include "heapsort.h"

void max_pq_init(MaxPriorityQueue *q, size_t capacity) {
  CLRS_ASSERT(capacity > 0, "capacity must be > 0");
  q->a = clrs_xcalloc(capacity, sizeof(int));
  q->capacity = capacity;
  q->heap_size = 0;
}

void max_pq_destroy(MaxPriorityQueue *q) {
  free(q->a);
  q->a = NULL;
  q->capacity = 0;
  q->heap_size = 0;
}

int max_pq_maximum(const MaxPriorityQueue *q) {
  CLRS_ASSERT(q->heap_size > 0, "heap underflow");
  return q->a[0];
}

int max_pq_extract_max(MaxPriorityQueue *q) {
  CLRS_ASSERT(q->heap_size > 0, "heap underflow");
  int max = q->a[0];
  q->a[0] = q->a[q->heap_size - 1];
  q->heap_size--;
  max_heapify(q->a, q->heap_size, 0);
  return max;
}

void max_pq_increase_key(MaxPriorityQueue *q, size_t i, int new_key) {
  CLRS_ASSERT(i < q->heap_size, "index out of heap");
  CLRS_ASSERT(new_key >= q->a[i], "new key is smaller than current");
  q->a[i] = new_key;
  while (i > 0 && q->a[heap_parent(i)] < q->a[i]) {
    int tmp = q->a[i];
    q->a[i] = q->a[heap_parent(i)];
    q->a[heap_parent(i)] = tmp;
    i = heap_parent(i);
  }
}

void max_pq_insert(MaxPriorityQueue *q, int key) {
  CLRS_ASSERT(q->heap_size < q->capacity, "heap overflow");
  q->heap_size++;
  size_t i = q->heap_size - 1;
  /* Book inserts -infinity then HEAP-INCREASE-KEY; here we place the key
   * and sift up directly (equivalent when key is the final value). */
  q->a[i] = key;
  while (i > 0 && q->a[heap_parent(i)] < q->a[i]) {
    int tmp = q->a[i];
    q->a[i] = q->a[heap_parent(i)];
    q->a[heap_parent(i)] = tmp;
    i = heap_parent(i);
  }
}
