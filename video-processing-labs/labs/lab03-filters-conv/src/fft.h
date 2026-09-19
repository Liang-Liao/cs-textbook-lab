/* copied from lab02-time-frequency */
#ifndef LAB03_FFT_H
#define LAB03_FFT_H

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    double re;
    double im;
} cpx;

void fft_rec(cpx *data, int n);
void ifft_rec(cpx *data, int n);
void fft(cpx *data, int n);

#endif
