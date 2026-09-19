#include "aec.h"
#include "fft.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static unsigned g_s = 1;
static void seed(unsigned s) { g_s = s ? s : 1; }
static double ur(void) {
    unsigned x=g_s; x^=x<<13; x^=x>>17; x^=x<<5; g_s=x;
    return ((x&0xffffff)+0.5)/16777216.0;
}

void nlms_init(nlms *f, int L, double mu) {
    f->L = L; f->mu = mu;
    f->h = calloc((size_t)L, sizeof(double));
    f->xbuf = calloc((size_t)L, sizeof(double));
}
void nlms_free(nlms *f) { free(f->h); free(f->xbuf); f->h=NULL; f->xbuf=NULL; }
void nlms_reset(nlms *f) {
    memset(f->h, 0, (size_t)f->L * sizeof(double));
    memset(f->xbuf, 0, (size_t)f->L * sizeof(double));
}

double nlms_step(nlms *f, double x, double d, int freeze) {
    /* shift delay line: newest at 0 */
    memmove(f->xbuf + 1, f->xbuf, (size_t)(f->L - 1) * sizeof(double));
    f->xbuf[0] = x;
    double y = 0.0;
    for (int i = 0; i < f->L; i++) y += f->h[i] * f->xbuf[i];
    double e = d - y;
    if (!freeze) {
        double px = 1e-8;
        for (int i = 0; i < f->L; i++) px += f->xbuf[i] * f->xbuf[i];
        double a = f->mu * e / px;
        for (int i = 0; i < f->L; i++) f->h[i] += a * f->xbuf[i];
    }
    return e;
}

int estimate_delay(const double *x, const double *d, int n, int max_delay) {
    /* FFT-based cross-correlation. Peak of IFFT(D * conj(X)) at τ where
     * d[n] ≈ x[n - τ] (d delayed relative to x). */
    if (!x || !d || n <= 0) return 0;
    if (max_delay <= 0 || max_delay >= n) max_delay = n / 4;
    int N = 1;
    while (N < 2 * n) N <<= 1;
    cpx *X = calloc((size_t)N, sizeof(cpx));
    cpx *Y = calloc((size_t)N, sizeof(cpx));
    if (!X || !Y) { free(X); free(Y); return 0; }
    for (int i = 0; i < n; i++) { X[i].re = x[i]; Y[i].re = d[i]; }
    fft(X, N); fft(Y, N);
    /* R = Y * conj(X) → r[m] = sum d[n] x[n-m]; peaks at m = delay */
    for (int i = 0; i < N; i++) {
        double re = Y[i].re * X[i].re + Y[i].im * X[i].im;
        double im = Y[i].im * X[i].re - Y[i].re * X[i].im;
        X[i].re = re; X[i].im = im;
    }
    ifft(X, N);
    int best = 0;
    double bestv = -1e300;
    for (int lag = 0; lag <= max_delay; lag++) {
        if (X[lag].re > bestv) { bestv = X[lag].re; best = lag; }
    }
    free(X); free(Y);
    return best;
}

double erle_db(const double *echo, const double *err, int n) {
    double pe = 0, pr = 0;
    for (int i = 0; i < n; i++) { pe += err[i]*err[i]; pr += echo[i]*echo[i]; }
    if (pe <= 0) return 200;
    return 10.0 * log10(pr / pe);
}

void synth_path(double *h, int L, double tau, unsigned seedv) {
    seed(seedv);
    /* Strong first tap + weak tail so the xcorr peak stays at the bulk delay
     * (a heavy exponential tail shifts the peak by several samples of group delay). */
    h[0] = 1.0;
    for (int i = 1; i < L; i++) {
        h[i] = 0.04 * exp(-(double)i / tau) * (0.25 + 0.75 * ur());
    }
}

void nlp_init(nlp *n, double floor_g, double resid_ratio) {
    n->echo_sm = 1e-8;
    n->err_sm = 1e-8;
    n->floor_g = floor_g;
    n->resid_ratio = resid_ratio;
}

double nlp_step(nlp *n, double echo_est, double e, int far_active) {
    /* slow echo smoother, faster err smoother → pass near-end onsets quickly */
    n->echo_sm = 0.998 * n->echo_sm + 0.002 * (echo_est * echo_est);
    n->err_sm = 0.97 * n->err_sm + 0.03 * (e * e);
    double g = 1.0;
    if (far_active) {
        /* r ≈ 1 means the error sits at the expected residual-echo level of a
         * converged AEC → suppress; r >> 1 means near-end energy → pass. */
        double exp_resid = n->resid_ratio * n->echo_sm + 1e-12;
        double r = n->err_sm / exp_resid;
        if (r <= 0.3) {
            g = n->floor_g;
        } else if (r < 1.0) {
            g = n->floor_g + (1.0 - n->floor_g) * (r - 0.3) / 0.7;
        }
    }
    return e * g;
}

/* ---------- double-talk detector (energy ratio + minimum statistics) ---------- */

void dt_init(dt_det *d, int blk, int hist, double k_ratio, double r_min,
             int max_stay) {
    d->blk = blk;
    d->hist = hist;
    d->mins = calloc((size_t)hist, sizeof(double));
    for (int i = 0; i < hist; i++) d->mins[i] = 1e30;
    d->acc_e2 = d->acc_y2 = 0.0;
    d->nacc = 0;
    d->beta = 1e30;
    d->k_ratio = k_ratio;
    d->r_min = r_min;
    d->max_stay = max_stay;
    d->pos = 0;
    d->dt = 0;
    d->below = 0;
    d->stay = 0;
    d->probing = 0;
    d->r_prev = 0.0;
}

void dt_free(dt_det *d) {
    free(d->mins);
    d->mins = NULL;
}

int dt_step(dt_det *d, double echo_est, double e) {
    /* pooled block energies, not per-sample ratios: a per-sample e²/ŷ²
     * explodes when the far signal dips toward zero and the ratio average
     * becomes heavy-tailed garbage */
    d->acc_e2 += e * e;
    d->acc_y2 += echo_est * echo_est;
    if (++d->nacc < d->blk) return d->dt;

    double r_blk = d->acc_e2 / (d->acc_y2 + 1e-9);
    double echo_blk = d->acc_y2 / d->blk;
    d->acc_e2 = d->acc_y2 = 0.0;
    d->nacc = 0;
    d->mins[d->pos] = r_blk;
    d->pos = (d->pos + 1) % d->hist;
    double bmin = 1e30;
    for (int i = 0; i < d->hist; i++) if (d->mins[i] < bmin) bmin = d->mins[i];
    d->beta = bmin;

    if (echo_blk > 1e-6) {
        double thr = (d->k_ratio * d->beta > d->r_min) ? d->k_ratio * d->beta : d->r_min;
        int above = r_blk > thr;
        if (d->probing) {
            /* probe outcome: a clearly falling residual means the filter was
             * misconverged (recovery works → keep adapting); a flat residual
             * above threshold means real double talk → re-freeze. */
            if (r_blk < 0.7 * d->r_prev) {
                d->dt = 0;
                d->below = 0;
                d->stay = 0;
                if (!above) d->probing = 0;
            } else if (above) {
                d->probing = 0;
                d->dt = 1;
                d->below = 0;
                d->stay = 0;
            } else {
                /* flat but below threshold: converged, exit normally */
                d->probing = 0;
                d->dt = 0;
                d->stay = 0;
            }
        } else if (above) {
            d->dt = 1;
            d->below = 0;
            d->stay++;
            if (d->stay >= d->max_stay) {
                d->probing = 1;
                d->dt = 0;
                d->stay = 0;
            }
        } else if (++d->below >= 3) {
            d->dt = 0;
            d->stay = 0;
        } else {
            d->stay++;
        }
    }
    d->r_prev = r_blk;
    return d->dt;
}

/* ---------- iterative radix-2 FFT (in-lab evolution of the lab02 recursive
 * FFT: the PBFDAF runs ~11 FFTs per block, where the recursive version's
 * call overhead dominates the whole time budget) ---------- */

static void fft_iter(cpx *a, int n, int inverse) {
    /* bit-reversal permutation */
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { cpx t = a[i]; a[i] = a[j]; a[j] = t; }
    }
    for (int len = 2; len <= n; len <<= 1) {
        double ang = (inverse ? 2.0 : -2.0) * M_PI / len;
        double wr = cos(ang), wi = sin(ang);
        for (int i = 0; i < n; i += len) {
            double cr = 1.0, ci = 0.0;
            for (int k = 0; k < len / 2; k++) {
                cpx u = a[i + k];
                cpx *v = &a[i + k + len / 2];
                double vr = v->re * cr - v->im * ci;
                double vi = v->re * ci + v->im * cr;
                v->re = u.re - vr; v->im = u.im - vi;
                a[i + k].re = u.re + vr; a[i + k].im = u.im + vi;
                double ncr = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = ncr;
            }
        }
    }
    if (inverse) {
        double s = 1.0 / n;
        for (int i = 0; i < n; i++) { a[i].re *= s; a[i].im *= s; }
    }
}

/* ---------- PBFDAF (overlap-save, P partitions) ---------- */

void pbfdaf_init(pbfdaf *f, int N, int P, double mu) {
    memset(f, 0, sizeof(*f));
    f->N = N; /* FFT size = 2 * block */
    f->P = P;
    f->mu = mu;
    f->Xf = calloc((size_t)P * N * 2, sizeof(double));
    f->H = calloc((size_t)P * N * 2, sizeof(double));
    f->xold = calloc((size_t)(N / 2), sizeof(double));
    f->xf = calloc((size_t)N * 2, sizeof(double));
    f->Yf = calloc((size_t)N * 2, sizeof(double));
    f->Ef = calloc((size_t)N * 2, sizeof(double));
    f->Ptot = calloc((size_t)N, sizeof(double));
}

void pbfdaf_free(pbfdaf *f) {
    free(f->Xf); free(f->H); free(f->xold); free(f->xf);
    free(f->Yf); free(f->Ef); free(f->Ptot);
    f->Xf = f->H = f->xold = f->xf = NULL;
    f->Yf = f->Ef = f->Ptot = NULL;
}

void pbfdaf_reset(pbfdaf *f) {
    if (f->Xf) memset(f->Xf, 0, (size_t)f->P * f->N * 2 * sizeof(double));
    if (f->H) memset(f->H, 0, (size_t)f->P * f->N * 2 * sizeof(double));
    if (f->xold) memset(f->xold, 0, (size_t)(f->N / 2) * sizeof(double));
}

void pbfdaf_process(pbfdaf *f, const double *far, const double *mic, double *err) {
    const int N = f->N, P = f->P, M = N / 2;
    cpx *X = (cpx *)f->xf;

    /* xbuf = [old M | new M] */
    for (int i = 0; i < M; i++) { X[i].re = f->xold[i]; X[i].im = 0.0; }
    for (int i = 0; i < M; i++) { X[M + i].re = far[i]; X[M + i].im = 0.0; }
    memcpy(f->xold, far, (size_t)M * sizeof(double));
    fft_iter(X, N, 0);

    /* shift partition delay line, newest at p=0 */
    if (P > 1)
        memmove(f->Xf + 2 * N, f->Xf, (size_t)(P - 1) * N * 2 * sizeof(double));
    for (int k = 0; k < N; k++) {
        f->Xf[2 * k] = X[k].re;
        f->Xf[2 * k + 1] = X[k].im;
    }

    /* Y = sum_p H_p X_p  (complex mul) */
    cpx *Y = (cpx *)f->Yf;
    memset(Y, 0, (size_t)N * sizeof(cpx));
    for (int p = 0; p < P; p++) {
        const double *Hp = f->H + (size_t)p * N * 2;
        const double *Xp = f->Xf + (size_t)p * N * 2;
        for (int k = 0; k < N; k++) {
            double xr = Xp[2 * k], xi = Xp[2 * k + 1];
            double hr = Hp[2 * k], hi = Hp[2 * k + 1];
            Y[k].re += hr * xr - hi * xi;
            Y[k].im += hr * xi + hi * xr;
        }
    }
    fft_iter(Y, N, 1);
    for (int i = 0; i < M; i++) err[i] = mic[i] - Y[M + i].re;
    /* E = FFT([0 | e]) */
    cpx *E = (cpx *)f->Ef;
    memset(E, 0, (size_t)N * sizeof(cpx));
    for (int i = 0; i < M; i++) { E[M + i].re = err[i]; }
    fft_iter(E, N, 0);

    /* bin-wise normalized update + per-partition time constraint.
     * Normalization uses the total input power across partitions
     * (standard MDF form — per-partition normalization destabilizes
     * partitions whose own block power is small). */
    double *ptot = f->Ptot;
    memset(ptot, 0, (size_t)N * sizeof(double));
    for (int p = 0; p < P; p++) {
        const double *Xp = f->Xf + (size_t)p * N * 2;
        for (int k = 0; k < N; k++) {
            double xr = Xp[2 * k], xi = Xp[2 * k + 1];
            ptot[k] += xr * xr + xi * xi;
        }
    }
    for (int k = 0; k < N; k++) ptot[k] += 1e-6;
    for (int p = 0; p < P; p++) {
        double *Hp = f->H + (size_t)p * N * 2;
        const double *Xp = f->Xf + (size_t)p * N * 2;
        for (int k = 0; k < N; k++) {
            double xr = Xp[2 * k], xi = Xp[2 * k + 1];
            double pwr = ptot[k];
            double er = E[k].re, ei = E[k].im;
            double gr = (er * xr + ei * xi) / pwr;
            double gi = (ei * xr - er * xi) / pwr;
            Hp[2 * k] += f->mu * gr;
            Hp[2 * k + 1] += f->mu * gi;
        }
        /* time-domain constraint each block (P is small) */
        cpx *h = (cpx *)Hp;
        fft_iter(h, N, 1);
        for (int i = M; i < N; i++) { h[i].re = 0.0; h[i].im = 0.0; }
        fft_iter(h, N, 0);
    }
}
