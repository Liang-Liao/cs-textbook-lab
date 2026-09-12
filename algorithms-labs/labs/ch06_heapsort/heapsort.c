#include "heapsort.h"

size_t heap_parent(size_t i) { return (i == 0) ? 0 : (i - 1) / 2; }

size_t heap_left(size_t i) { return 2 * i + 1; }

size_t heap_right(size_t i) { return 2 * i + 2; }

void max_heapify(int *a, size_t heap_size, size_t i) {
  for (;;) {
    size_t l = heap_left(i);
    size_t r = heap_right(i);
    size_t largest = i;

    if (l < heap_size && a[l] > a[largest]) {
      largest = l;
    }
    if (r < heap_size && a[r] > a[largest]) {
      largest = r;
    }
    if (largest == i) {
      break;
    }
    int tmp = a[i];
    a[i] = a[largest];
    a[largest] = tmp;
    i = largest;
  }
}

void build_max_heap(int *a, size_t n) {
  if (n < 2) {
    return;
  }
  /* Last non-leaf: parent of n-1 */
  size_t start = heap_parent(n - 1);
  /* Careful: heap_parent(0)=0, but leaf at 0 when n==1 handled above */
  for (size_t i = start + 1; i-- > 0;) {
    max_heapify(a, n, i);
  }
}

void heapsort_int(int *a, size_t n) {
  if (n < 2) {
    return;
  }
  build_max_heap(a, n);
  size_t heap_size = n;
  for (size_t i = n - 1; i > 0; i--) {
    int tmp = a[0];
    a[0] = a[i];
    a[i] = tmp;
    heap_size--;
    max_heapify(a, heap_size, 0);
  }
}
