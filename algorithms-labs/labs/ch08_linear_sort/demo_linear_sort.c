/* Demo: counting / radix / bucket sort (CLRS Ch.8). */
#include <stdio.h>
#include <stdlib.h>

#include "array.h"
#include "bucket_sort.h"
#include "counting_sort.h"
#include "radix_sort.h"

int main(void) {
  printf("=== CLRS 8.2 Counting sort ===\n");
  int a[] = {2, 5, 3, 0, 2, 3, 0, 3};
  int b[8];
  array_print_int("input", a, 8);
  counting_sort_int(a, b, 8, 5);
  array_print_int("output", b, 8);

  printf("\n=== CLRS 8.3 Radix sort ===\n");
  int r[] = {329, 457, 657, 839, 436, 720, 355};
  array_print_int("input", r, 7);
  radix_sort_int(r, 7);
  array_print_int("output", r, 7);

  printf("\n=== CLRS 8.4 Bucket sort ===\n");
  double d[] = {0.78, 0.17, 0.39, 0.26, 0.72, 0.94, 0.21, 0.12, 0.23, 0.68};
  printf("input:  ");
  for (int i = 0; i < 10; i++) {
    printf("%.2f ", d[i]);
  }
  printf("\n");
  bucket_sort_double(d, 10);
  printf("output: ");
  for (int i = 0; i < 10; i++) {
    printf("%.2f ", d[i]);
  }
  printf("\n");

  return 0;
}
