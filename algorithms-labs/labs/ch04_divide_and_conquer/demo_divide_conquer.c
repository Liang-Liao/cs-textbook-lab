/* Demo: max subarray book example + small matrix multiply. */
#include <stdio.h>

#include "clrs.h"
#include "max_subarray.h"
#include "matrix_multiply.h"

int main(void) {
  long long book[] = {13,  -3, -25, 20,  -3, -16, -23, 18,
                      20,  -7, 12,  -5,  -22, 15, -4,  7};
  size_t n = sizeof(book) / sizeof(book[0]);

  printf("=== CLRS 4.1 Maximum subarray (Fig. 4.1) ===\n");
  printf("A = ");
  for (size_t i = 0; i < n; i++) {
    printf("%lld%s", book[i], i + 1 < n ? ", " : "\n");
  }

  MaxSubarray dc = max_subarray_dc(book, n);
  MaxSubarray br = max_subarray_brute(book, n);
  MaxSubarray ka = max_subarray_kadane(book, n);

  printf("divide-and-conquer: [%zu..%zu] sum=%lld\n", dc.low, dc.high, dc.sum);
  printf("brute O(n^2):       [%zu..%zu] sum=%lld\n", br.low, br.high, br.sum);
  printf("kadane O(n):        [%zu..%zu] sum=%lld\n", ka.low, ka.high, ka.sum);

  printf("\n=== CLRS 4.2 Matrix multiply (2x2) ===\n");
  long long A[] = {1, 3, 7, 5};
  long long B[] = {6, 8, 4, 2};
  long long C[4];
  matrix_multiply_naive(C, A, B, 2);
  printf("A * B = [[%lld, %lld], [%lld, %lld]]\n", C[0], C[1], C[2], C[3]);

  long long S[4];
  matrix_multiply_strassen(S, A, B, 2);
  printf("Strassen: [[%lld, %lld], [%lld, %lld]]\n", S[0], S[1], S[2], S[3]);

  return 0;
}
