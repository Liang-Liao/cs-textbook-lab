#ifndef CLRS_PRIORITY_QUEUE_H
#define CLRS_PRIORITY_QUEUE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS 6.5 Priority queues on a max-heap.
 * Fixed capacity; elements are int keys.
 */

typedef struct {
  int *a;        /* heap array */
  size_t capacity;
  size_t heap_size;
} MaxPriorityQueue;

void max_pq_init(MaxPriorityQueue *q, size_t capacity);
void max_pq_destroy(MaxPriorityQueue *q);

/* CLRS HEAP-MAXIMUM. Requires heap_size > 0. */
int max_pq_maximum(const MaxPriorityQueue *q);

/* CLRS HEAP-EXTRACT-MAX. Requires heap_size > 0. */
int max_pq_extract_max(MaxPriorityQueue *q);

/* CLRS HEAP-INCREASE-KEY at 0-based index i. new_key >= current. */
void max_pq_increase_key(MaxPriorityQueue *q, size_t i, int new_key);

/* CLRS MAX-HEAP-INSERT. */
void max_pq_insert(MaxPriorityQueue *q, int key);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_PRIORITY_QUEUE_H */
