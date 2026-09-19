/* copied from lab02-time-frequency (adapted for lab03-filters-conv) */
#include "fft.h"

#include <math.h>
#include <stdlib.h>

void fft_rec(cpx *data, int n) {
    if (!data || n <= 1) return;
    if ((n & (n - 1)) != 0) return;
    int half = n / 2;
    cpx *even = (cpx *)malloc((size_t)half * sizeof(cpx));
    cpx *odd = (cpx *)malloc((size_t)half * sizeof(cpx));
    if (!even || !odd) {
        free(even);
        free(odd);
        return;
    }
    for (int i = 0; i < half; i++) {
        even[i] = data[2 * i];
        odd[i] = data[2 * i + 1];
    }
    fft_rec(even, half);
    fft_rec(odd, half);
    for (int k = 0; k < half; k++) {
        double ang = -2.0 * M_PI * (double)k / (double)n;
        double wr = cos(ang), wi = sin(ang);
        double tr = wr * odd[k].re - wi * odd[k].im;
        double ti = wr * odd[k].im + wi * odd[k].re;
        data[k].re = even[k].re + tr;
        data[k].im = even[k].im + ti;
        data[k + half].re = even[k].re - tr;
        data[k + half].im = even[k].im - ti;
    }
    free(even);
    free(odd);
}

void ifft_rec(cpx *data, int n) {
    if (!data || n <= 1) return;
    if ((n & (n - 1)) != 0) return;
    for (int i = 0; i < n; i++) data[i].im = -data[i].im;
    fft_rec(data, n);
    for (int i = 0; i < n; i++) {
        data[i].re /= (double)n;
        data[i].im = -data[i].im / (double)n;
    }
}

void fft(cpx *data, int n) { fft_rec(data, n); }
