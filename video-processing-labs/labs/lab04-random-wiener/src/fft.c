#include "fft.h"
#include <math.h>
#include <stdlib.h>

void fft(cpx *data, int n) {
    if (!data || n <= 1) return;
    int half = n / 2;
    cpx *e = malloc((size_t)half * sizeof(cpx));
    cpx *o = malloc((size_t)half * sizeof(cpx));
    if (!e || !o) { free(e); free(o); return; }
    for (int i = 0; i < half; i++) { e[i] = data[2 * i]; o[i] = data[2 * i + 1]; }
    fft(e, half); fft(o, half);
    for (int k = 0; k < half; k++) {
        double a = -2.0 * M_PI * k / n;
        double wr = cos(a), wi = sin(a);
        double tr = wr * o[k].re - wi * o[k].im;
        double ti = wr * o[k].im + wi * o[k].re;
        data[k].re = e[k].re + tr; data[k].im = e[k].im + ti;
        data[k + half].re = e[k].re - tr; data[k + half].im = e[k].im - ti;
    }
    free(e); free(o);
}

void ifft(cpx *data, int n) {
    for (int i = 0; i < n; i++) data[i].im = -data[i].im;
    fft(data, n);
    for (int i = 0; i < n; i++) {
        data[i].re /= n;
        data[i].im = -data[i].im / n;
    }
}
