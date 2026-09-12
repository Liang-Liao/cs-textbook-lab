#ifndef CLRS_HEAPSORT_H
#define CLRS_HEAPSORT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS Ch.6 Heapsort — max-heaps stored as 0-based arrays.
 * Book uses 1-based indices; children of i are 2i+1 and 2i+2, parent (i-1)/2.
 */

/* PARENT / LEFT / RIGHT helpers */
size_t heap_parent(size_t i);
size_t heap_left(size_t i);
size_t heap_right(size_t i);

/* CLRS 6.2 MAX-HEAPIFY. a is a heap-sized array; subtree at i may violate. */
void max_heapify(int *a, size_t heap_size, size_t i);

/* CLRS 6.3 BUILD-MAX-HEAP. heap_size = n. */
void build_max_heap(int *a, size_t n);

/* CLRS 6.4 HEAPSORT. Sorts a[0..n) ascending, in place. */
void heapsort_int(int *a, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_HEAPSORT_H */
