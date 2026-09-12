#ifndef CLRS_QUEUE_H
#define CLRS_QUEUE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS 10.1 Queues — circular array, fixed capacity.
 * Can hold at most capacity-1 elements (one slot distinguishes full/empty),
 * matching the book's Q.head/Q.tail convention.
 */
typedef struct {
  int *a;
  size_t capacity;
  size_t head; /* index of next dequeue */
  size_t tail; /* index of next enqueue */
  size_t count;
} IntQueue;

void queue_init(IntQueue *q, size_t capacity);
void queue_destroy(IntQueue *q);
int queue_empty(const IntQueue *q);
int queue_full(const IntQueue *q);
void queue_enqueue(IntQueue *q, int x);
int queue_dequeue(IntQueue *q);
int queue_head(const IntQueue *q);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_QUEUE_H */
