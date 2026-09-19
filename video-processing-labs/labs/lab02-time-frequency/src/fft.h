#ifndef LAB02_FFT_H
#define LAB02_FFT_H

#include <stddef.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    double re;
    double im;
} cpx;

void dft(const cpx *in, cpx *out, int n);
void fft_rec(cpx *data, int n);
void ifft_rec(cpx *data, int n);
void fft(cpx *data, int n);

double rel_max_err(const cpx *a, const cpx *b, int n);
double wall_ms(void);

#endif
