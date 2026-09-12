#ifndef CLRS_FIB_HEAP_H
#define CLRS_FIB_HEAP_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS Ch.19 Fibonacci heap (min-heap).
 * Nodes hold int keys; decrease-key requires a node handle.
 */

typedef struct FibNode {
  int key;
  int degree;
  int mark;
  struct FibNode *parent;
  struct FibNode *child;
  struct FibNode *left;  /* circular sibling list */
  struct FibNode *right;
} FibNode;

typedef struct {
  FibNode *min;
  size_t n; /* total nodes */
} FibHeap;

void fib_init(FibHeap *h);
void fib_destroy(FibHeap *h);

/* CLRS FIB-HEAP-INSERT. Returns node pointer. */
FibNode *fib_insert(FibHeap *h, int key);

/* CLRS FIB-HEAP-EXTRACT-MIN. Returns extracted key; heap must be nonempty. */
int fib_extract_min(FibHeap *h);

int fib_minimum(const FibHeap *h);

/* CLRS FIB-HEAP-DECREASE-KEY. new_key < node->key. */
void fib_decrease_key(FibHeap *h, FibNode *x, int new_key);

/* CLRS FIB-HEAP-UNION: move all nodes of h2 into h1; h2 becomes empty. */
void fib_union(FibHeap *h1, FibHeap *h2);

size_t fib_size(const FibHeap *h);

/* Collect all keys (unsorted). Returns count. */
size_t fib_collect(const FibHeap *h, int *out, size_t maxn);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_FIB_HEAP_H */
