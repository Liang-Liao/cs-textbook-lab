#include "queue.h"

#include <stdlib.h>

#include "clrs.h"

void queue_init(IntQueue *q, size_t capacity) {
  CLRS_ASSERT(capacity > 1, "capacity must be > 1");
  q->a = clrs_xmalloc(capacity * sizeof(int));
  q->capacity = capacity;
  q->head = 0;
  q->tail = 0;
  q->count = 0;
}

void queue_destroy(IntQueue *q) {
  free(q->a);
  q->a = NULL;
  q->capacity = 0;
  q->head = q->tail = q->count = 0;
}

int queue_empty(const IntQueue *q) { return q->count == 0; }

int queue_full(const IntQueue *q) { return q->count == q->capacity - 1; }

void queue_enqueue(IntQueue *q, int x) {
  CLRS_ASSERT(!queue_full(q), "queue overflow");
  q->a[q->tail] = x;
  q->tail = (q->tail + 1) % q->capacity;
  q->count++;
}

int queue_dequeue(IntQueue *q) {
  CLRS_ASSERT(!queue_empty(q), "queue underflow");
  int x = q->a[q->head];
  q->head = (q->head + 1) % q->capacity;
  q->count--;
  return x;
}

int queue_head(const IntQueue *q) {
  CLRS_ASSERT(!queue_empty(q), "queue empty");
  return q->a[q->head];
}
