/* Demo: print sorted results on a small array. */
#include <stdio.h>

#include "array.h"
#include "insertion_sort.h"
#include "merge_sort.h"

int main(void) {
  int a[] = {5, 2, 4, 6, 1, 3};
  const size_t n = sizeof(a) / sizeof(a[0]);

  int b[6];
  array_copy_int(b, a, n);

  array_print_int("input", a, n);

  insertion_sort_int(a, n);
  array_print_int("insertion_sort", a, n);

  merge_sort_int(b, n);
  array_print_int("merge_sort", b, n);

  return 0;
}
