/* copied from lab02-time-frequency */
#ifndef LAB06_FFT_H
#define LAB06_FFT_H
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
typedef struct {
    double re, im;
} cpx;
void fft(cpx *data, int n);
void ifft(cpx *data, int n);
#endif
