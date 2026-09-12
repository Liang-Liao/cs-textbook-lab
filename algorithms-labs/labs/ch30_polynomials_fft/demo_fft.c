/* Demo: polynomial multiply via FFT (CLRS Ch.30). */
#include <stdio.h>

#include "poly_fft.h"

int main(void) {
  printf("=== CLRS 30 Polynomial multiply via FFT ===\n");
  double A[] = {1, 2, 3, 0}; /* 1 + 2x + 3x^2 */
  double B[] = {4, 5, 6, 0}; /* 4 + 5x + 6x^2 */
  double out[8];
  poly_multiply_fft(A, B, 4, out);

  printf("A(x) = 1 + 2x + 3x^2\n");
  printf("B(x) = 4 + 5x + 6x^2\n");
  printf("A*B  =");
  for (int i = 0; i < 5; i++) {
    printf(" %g x^%d", out[i], i);
  }
  printf("\n");

  double a2[] = {1, 1};
  double b2[] = {1, 1};
  double c2[4];
  poly_multiply_fft(a2, b2, 2, c2);
  printf("(1+x)^2 = %g + %g x + %g x^2\n", c2[0], c2[1], c2[2]);
  return 0;
}
