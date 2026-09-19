#include "fft.h"

#include <math.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

double wall_ms(void) {
#ifdef _WIN32
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return 1000.0 * (double)c.QuadPart / (double)f.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return 1000.0 * (double)ts.tv_sec + 1e-6 * (double)ts.tv_nsec;
#endif
}

void dft(const cpx *in, cpx *out, int n) {
    for (int k = 0; k < n; k++) {
        double re = 0.0, im = 0.0;
        for (int t = 0; t < n; t++) {
            double ang = -2.0 * M_PI * (double)k * (double)t / (double)n;
            double c = cos(ang), s = sin(ang);
            re += in[t].re * c - in[t].im * s;
            im += in[t].re * s + in[t].im * c;
        }
        out[k].re = re;
        out[k].im = im;
    }
}

static int is_pow2(int n) { return n > 0 && (n & (n - 1)) == 0; }

void fft_rec(cpx *data, int n) {
    if (!data || n <= 1) return;
    if (!is_pow2(n)) return;
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
    if (!data || !is_pow2(n)) return;
    for (int i = 0; i < n; i++) data[i].im = -data[i].im;
    fft_rec(data, n);
    for (int i = 0; i < n; i++) {
        data[i].re /= (double)n;
        data[i].im = -data[i].im / (double)n;
    }
}

void fft(cpx *data, int n) { fft_rec(data, n); }

double rel_max_err(const cpx *a, const cpx *b, int n) {
    double max_b = 1e-300, max_e = 0.0;
    for (int i = 0; i < n; i++) {
        double bm = sqrt(b[i].re * b[i].re + b[i].im * b[i].im);
        if (bm > max_b) max_b = bm;
        double er = a[i].re - b[i].re, ei = a[i].im - b[i].im;
        double em = sqrt(er * er + ei * ei);
        if (em > max_e) max_e = em;
    }
    return max_e / max_b;
}
