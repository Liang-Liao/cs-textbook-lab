/* copied from lab05-audio-features (adapted for lab09-agc) */
#include "lufs.h"
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Two simple biquads approximating K-weighting (high shelf + highpass). */
typedef struct { double b0,b1,b2,a1,a2; } bq;

static void bq_hs(bq *b, double fs) {
    /* RBJ high shelf ~+4dB @ 1681 Hz, Q=0.7071 — simplified */
    double f0 = 1681.97, Q = 0.7071752, A = pow(10.0, 3.999843853978935/40.0);
    double w0 = 2*M_PI*f0/fs, c=cos(w0), s=sin(w0), al=s/(2*Q), be=2*sqrt(A)*al;
    double b0=A*((A+1)+(A-1)*c+be), b1=-2*A*((A-1)+(A+1)*c), b2=A*((A+1)+(A-1)*c-be);
    double a0=(A+1)-(A-1)*c+be, a1=2*((A-1)-(A+1)*c), a2=(A+1)-(A-1)*c-be;
    b->b0=b0/a0; b->b1=b1/a0; b->b2=b2/a0; b->a1=a1/a0; b->a2=a2/a0;
}
static void bq_hp(bq *b, double fs) {
    double f0 = 38.13547087602444, Q = 0.5003270373238773;
    double w0=2*M_PI*f0/fs, c=cos(w0), s=sin(w0), al=s/(2*Q);
    double b0=(1+c)/2, b1=-(1+c), b2=(1+c)/2, a0=1+al, a1=-2*c, a2=1-al;
    b->b0=b0/a0; b->b1=b1/a0; b->b2=b2/a0; b->a1=a1/a0; b->a2=a2/a0;
}
static double bq_step(const bq *b, double x, double *z1, double *z2) {
    double y = b->b0*x + *z1;
    *z1 = b->b1*x - b->a1*y + *z2;
    *z2 = b->b2*x - b->a2*y;
    return y;
}

double lufs_integrated(const double *x, int n, double fs) {
    bq hs, hp;
    bq_hs(&hs, fs); bq_hp(&hp, fs);
    double z1=0,z2=0,z3=0,z4=0;
    /* 400 ms blocks, 75% overlap */
    int bl = (int)(0.4 * fs);
    int hop = bl / 4;
    if (bl < 16) return -70.0;
    double *block_pow = NULL;
    int nb = 0, cap = n / hop + 2;
    block_pow = malloc((size_t)cap * sizeof(double));
    if (!block_pow) return -70.0;
    for (int pos = 0; pos + bl <= n; pos += hop) {
        double s = 0;
        /* filter once into temp on the fly with state — approximate using
           filtered stream stored on first pass */
        for (int i = 0; i < bl; i++) {
            double y = bq_step(&hs, x[pos+i], &z1, &z2);
            y = bq_step(&hp, y, &z3, &z4);
            s += y * y;
        }
        block_pow[nb++] = s / bl;
    }
    if (nb == 0) { free(block_pow); return -70.0; }
    /* absolute gate -70 LUFS */
    double sum = 0; int cnt = 0;
    for (int i = 0; i < nb; i++) {
        double l = -0.691 + 10.0 * log10(block_pow[i] + 1e-30);
        if (l > -70.0) { sum += block_pow[i]; cnt++; }
    }
    if (cnt == 0) { free(block_pow); return -70.0; }
    double meanp = sum / cnt;
    double rel_gate = -0.691 + 10.0 * log10(meanp + 1e-30) - 10.0;
    sum = 0; cnt = 0;
    for (int i = 0; i < nb; i++) {
        double l = -0.691 + 10.0 * log10(block_pow[i] + 1e-30);
        if (l > rel_gate && l > -70.0) { sum += block_pow[i]; cnt++; }
    }
    free(block_pow);
    if (cnt == 0) return -70.0;
    return -0.691 + 10.0 * log10(sum / cnt + 1e-30);
}
