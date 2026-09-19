/* copied from lab02-time-frequency */
#ifndef LAB07_FFT_H
#define LAB07_FFT_H
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
typedef struct { double re, im; } cpx;
void fft(cpx *d, int n);
void ifft(cpx *d, int n);
#endif
