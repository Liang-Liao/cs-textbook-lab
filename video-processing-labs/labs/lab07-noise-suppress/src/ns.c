#include "ns.h"
#include "stft.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

void dd_wiener_gain(const double *Y2, const double *N2, double *xi, int n, double *G) {
    const double alpha = 0.90; /* faster DD tracking so speech bins reach G≈1 */
    for (int k = 0; k < n; k++) {
        double gamma = (N2[k] > 1e-30) ? (Y2[k] / N2[k]) : 1e6;
        double gi = (Y2[k] > 1e-30) ? (xi[k] / (1.0 + xi[k])) : 0.0;
        double xin = alpha * gi + (1.0 - alpha) * fmax(gamma - 1.0, 0.0);
        xi[k] = xin;
        G[k] = xin / (1.0 + xin);
        if (G[k] < 0.03) G[k] = 0.03; /* spectral floor */
    }
}

void spectral_subtract_mag(double *mag, const double *Nmag, int n, double alpha, double beta) {
    for (int k = 0; k < n; k++) {
        double v = mag[k] - alpha * Nmag[k];
        if (v < beta * Nmag[k]) v = beta * Nmag[k];
        mag[k] = v;
    }
}

void minstat_init(minstat *m, int n, int win) {
    m->n = n; m->win = win; m->pos = 0;
    m->buf = calloc((size_t)win * n, sizeof(double));
    m->N2 = calloc((size_t)n, sizeof(double));
}
void minstat_free(minstat *m) {
    free(m->buf); free(m->N2);
    m->buf = NULL; m->N2 = NULL;
}
void minstat_update(minstat *m, const double *Y2) {
    memcpy(m->buf + (size_t)m->pos * m->n, Y2, (size_t)m->n * sizeof(double));
    m->pos = (m->pos + 1) % m->win;
    for (int k = 0; k < m->n; k++) {
        double mn = 1e300;
        for (int i = 0; i < m->win; i++) {
            double v = m->buf[(size_t)i * m->n + k];
            if (v < mn) mn = v;
        }
        /* Only pull toward the window minimum; do not inflate above the
         * running floor (speech bins would otherwise raise N2). The pipeline
         * only feeds non-speech frames, so the up-rate can be brisk enough
         * to follow a +10 dB step within ~1 s. */
        if (mn < m->N2[k]) m->N2[k] = 0.7 * m->N2[k] + 0.3 * mn;
        else m->N2[k] = 0.95 * m->N2[k] + 0.05 * mn;
    }
}

void ns_estimate_noise(const double *noisy, int n, int nfft, int hop,
                       int n_init, double *N2) {
    memset(N2, 0, (size_t)nfft * sizeof(double));
    if (n < nfft || hop <= 0) return;
    int maxf = 1 + (n - nfft) / hop;
    if (n_init > maxf) n_init = maxf;
    if (n_init < 1) n_init = 1;

    double *w = malloc((size_t)nfft * sizeof(double));
    cpx *X = malloc((size_t)nfft * sizeof(cpx));
    if (!w || !X) { free(w); free(X); return; }
    hann(w, nfft);

    /* Stage 1: average first n_init (noise-only) frames. */
    for (int f = 0; f < n_init; f++) {
        int s0 = f * hop;
        for (int i = 0; i < nfft; i++) {
            X[i].re = noisy[s0 + i] * w[i];
            X[i].im = 0.0;
        }
        fft(X, nfft);
        for (int k = 0; k < nfft; k++)
            N2[k] += X[k].re * X[k].re + X[k].im * X[k].im;
    }
    for (int k = 0; k < nfft; k++) N2[k] /= (double)n_init;

    /* Noise-only prefix is the estimator for this lab (synthetic corpus starts
     * with silence). Online min-statistics is exercised separately in
     * test_minstat_track so speech frames cannot inflate N2 here. */
    free(w); free(X);
}

void ns_process_frames(cpx *frames, int nf, int nfft, double *N2, double floor_g) {
    int bins = nfft;
    double *Y2 = calloc((size_t)bins, sizeof(double));
    double *xi = calloc((size_t)bins, sizeof(double));
    double *G = calloc((size_t)bins, sizeof(double));
    if (!Y2 || !xi || !G) { free(Y2); free(xi); free(G); return; }
    for (int f = 0; f < nf; f++) {
        cpx *X = frames + (size_t)f * nfft;
        for (int k = 0; k < bins; k++)
            Y2[k] = X[k].re * X[k].re + X[k].im * X[k].im;
        if (f == 0) {
            /* seed prior SNR from the first frame so DD does not start blind */
            for (int k = 0; k < bins; k++) {
                double g = (N2[k] > 1e-30) ? (Y2[k] / N2[k]) : 1e6;
                xi[k] = fmax(g - 1.0, 0.0);
            }
        }
        dd_wiener_gain(Y2, N2, xi, bins, G);
        for (int k = 0; k < bins; k++) {
            double g = G[k];
            /* bin-level speech protection: only high local-SNR bins */
            if (N2[k] > 1e-30 && Y2[k] > 4.0 * N2[k] && g < 0.92) g = 0.92;
            if (g < floor_g) g = floor_g;
            X[k].re *= g;
            X[k].im *= g;
        }
    }
    free(Y2); free(xi); free(G);
}

void ns_process_spectral_sub(cpx *frames, int nf, int nfft, const double *N2,
                             double alpha, double beta) {
    double *Nmag = malloc((size_t)nfft * sizeof(double));
    double *mag = malloc((size_t)nfft * sizeof(double));
    if (!Nmag || !mag) { free(Nmag); free(mag); return; }
    for (int k = 0; k < nfft; k++) Nmag[k] = sqrt(fmax(N2[k], 0.0));
    for (int f = 0; f < nf; f++) {
        cpx *X = frames + (size_t)f * nfft;
        for (int k = 0; k < nfft; k++)
            mag[k] = sqrt(X[k].re * X[k].re + X[k].im * X[k].im);
        spectral_subtract_mag(mag, Nmag, nfft, alpha, beta);
        for (int k = 0; k < nfft; k++) {
            double p = sqrt(X[k].re * X[k].re + X[k].im * X[k].im);
            double s = (p > 1e-30) ? (mag[k] / p) : 0.0;
            X[k].re *= s;
            X[k].im *= s;
        }
    }
    free(Nmag); free(mag);
}

/* McAulay–Malpass per-bin likelihood ratio under H1 (speech) vs H0 (noise)
 * with prior SNR xi: LR = exp(γ·ξ/(1+ξ)) / (1+ξ). */
static double bin_lr(double gamma, double xi) {
    if (xi < 0.0) xi = 0.0;
    double v = gamma * xi / (1.0 + xi);
    if (v > 700.0) v = 700.0; /* exp overflow guard */
    return exp(v) / (1.0 + xi);
}

int ns_process_webrtc(cpx *frames, int nf, int nfft, double floor_g) {
    /* Simplified WebRTC-style online NS: LR/energy speech decision gates the
     * noise tracker (non-speech frames only) → DD prior SNR → Wiener gain →
     * gain floor. Noise tracking uses silence-frame means with
     * fast-down/slow-up EMAs: a hard window minimum ratchets low (the
     * minimum of exponential draws is biased to ~mean/win), which would
     * under-estimate the noise and collapse suppression. */
    int bins = nfft;
    double *N2 = calloc((size_t)bins, sizeof(double));
    double *Y2 = calloc((size_t)bins, sizeof(double));
    double *xi = calloc((size_t)bins, sizeof(double));
    double *G = calloc((size_t)bins, sizeof(double));
    if (!N2 || !Y2 || !xi || !G) {
        free(N2); free(Y2); free(xi); free(G); return 0;
    }

    /* Seed noise floor from the first frames (assumed noise-dominant). */
    int n_init = (nf < 8) ? nf : 8;
    for (int f = 0; f < n_init; f++) {
        cpx *X = frames + (size_t)f * bins;
        for (int k = 0; k < bins; k++) {
            double p = X[k].re * X[k].re + X[k].im * X[k].im;
            N2[k] += p;
        }
    }
    for (int k = 0; k < bins; k++) N2[k] /= (double)n_init;

    int speech_frames = 0;
    int seeded = 0;
    for (int f = 0; f < nf; f++) {
        cpx *X = frames + (size_t)f * bins;
        double e_y = 0.0;
        for (int k = 0; k < bins; k++) {
            Y2[k] = X[k].re * X[k].re + X[k].im * X[k].im;
            e_y += Y2[k];
        }

        if (!seeded) {
            for (int k = 0; k < bins; k++) {
                double g = (N2[k] > 1e-30) ? (Y2[k] / N2[k]) : 1e6;
                xi[k] = fmax(g - 1.0, 0.0);
            }
            seeded = 1;
        }

        /* frame speech decision: energy test OR LR confidence. The per-bin
         * McAulay–Malpass LR alone over-fires on chi-square fluctuations
         * (its expectation is convex in gamma), so it sharpens the energy
         * decision rather than replacing it — same multi-feature structure
         * as the WebRTC NS Vad. */
        double e_n = 0.0;
        for (int k = 0; k < bins; k++) e_n += N2[k];
        int p_hi = 0;
        for (int k = 0; k < bins; k++) {
            double gamma = (N2[k] > 1e-30) ? (Y2[k] / N2[k]) : 1e6;
            double lr = bin_lr(gamma, xi[k]);
            if (lr / (1.0 + lr) > 0.8) p_hi++;
        }
        double p_frac = (double)p_hi / bins;
        int is_speech = (e_y > 2.5 * e_n) || (p_frac > 0.25);
        if (is_speech) speech_frames++;

        /* online noise tracking on non-speech frames only: speech can never
         * inflate the floor and a mid-stream noise rise is still tracked
         * (0.05/frame reaches ~95% of a step in ~0.5 s) */
        if (!is_speech) {
            for (int k = 0; k < bins; k++) {
                if (Y2[k] < N2[k]) N2[k] += 0.1 * (Y2[k] - N2[k]);
                else N2[k] += 0.05 * (Y2[k] - N2[k]);
            }
        }

        dd_wiener_gain(Y2, N2, xi, bins, G);
        for (int k = 0; k < bins; k++) {
            double g = G[k];
            if (N2[k] > 1e-30 && Y2[k] > 4.0 * N2[k] && g < 0.92)
                g = 0.92; /* protect speech bins only */
            if (g < floor_g) g = floor_g;
            X[k].re *= g;
            X[k].im *= g;
        }
    }
    free(N2); free(Y2); free(xi); free(G);
    return speech_frames;
}

double snr_db(const double *ref, const double *test, int n) {
    double ps = 0, pe = 0;
    for (int i = 0; i < n; i++) {
        double e = test[i] - ref[i];
        ps += ref[i] * ref[i];
        pe += e * e;
    }
    if (pe <= 0) return 200;
    return 10.0 * log10(ps / pe);
}

double speech_damage_db(const double *clean, const double *noisy, const double *out,
                        int n, int nfft, int hop) {
    /* Energy ratio out/noisy restricted to speech-dominant bins on speech frames. */
    double *w = malloc((size_t)nfft * sizeof(double));
    cpx *Xc = malloc((size_t)nfft * sizeof(cpx));
    cpx *Xn = malloc((size_t)nfft * sizeof(cpx));
    cpx *Xo = malloc((size_t)nfft * sizeof(cpx));
    if (!w || !Xc || !Xn || !Xo) {
        free(w); free(Xc); free(Xn); free(Xo); return 0.0;
    }
    hann(w, nfft);
    double pin = 0.0, pout = 0.0;
    for (int pos = 0; pos + nfft <= n; pos += hop) {
        for (int i = 0; i < nfft; i++) {
            Xc[i].re = clean[pos + i] * w[i]; Xc[i].im = 0.0;
            Xn[i].re = noisy[pos + i] * w[i]; Xn[i].im = 0.0;
            Xo[i].re = out[pos + i] * w[i];   Xo[i].im = 0.0;
        }
        fft(Xc, nfft); fft(Xn, nfft); fft(Xo, nfft);
        double ec = 0.0;
        for (int k = 0; k < nfft; k++) {
            ec += Xc[k].re * Xc[k].re + Xc[k].im * Xc[k].im;
        }
        if (ec < 1e-8) continue; /* not a speech frame */
        for (int k = 0; k < nfft; k++) {
            double pc = Xc[k].re * Xc[k].re + Xc[k].im * Xc[k].im;
            double pn = Xn[k].re * Xn[k].re + Xn[k].im * Xn[k].im;
            double po = Xo[k].re * Xo[k].re + Xo[k].im * Xo[k].im;
            /* speech-dominant bin (aligned with γ>4 protection): clean ≥75% of noisy */
            if (pc > 0.75 * pn && pc > 1e-10) {
                pin += pn;
                pout += po;
            }
        }
    }
    free(w); free(Xc); free(Xn); free(Xo);
    if (pin <= 1e-30) return 0.0;
    return 10.0 * log10(pout / pin);
}

void spectrogram_to_pgm(const double *sig, int n, int nfft, int hop,
                        uint8_t *buf, int w, int h) {
    if (!sig || !buf || nfft <= 0 || hop <= 0 || w <= 0 || h <= 0) return;
    double *win = malloc((size_t)nfft * sizeof(double));
    cpx *frame = malloc((size_t)nfft * sizeof(cpx));
    if (!win || !frame) { free(win); free(frame); return; }
    hann(win, nfft);
    int bins = nfft / 2;
    for (int col = 0; col < w; col++) {
        int start = (int)((long long)col * (n - nfft) / (w > 1 ? w - 1 : 1));
        if (start < 0) start = 0;
        for (int i = 0; i < nfft; i++) {
            int idx = start + i;
            double s = (idx >= 0 && idx < n) ? sig[idx] : 0.0;
            frame[i].re = s * win[i];
            frame[i].im = 0.0;
        }
        fft(frame, nfft);
        for (int row = 0; row < h; row++) {
            double t = (double)row / (double)(h > 1 ? h - 1 : 1);
            int b = (int)lrint((1.0 - t) * (double)(bins - 1));
            if (b < 0) b = 0;
            if (b >= bins) b = bins - 1;
            double m = sqrt(frame[b].re * frame[b].re + frame[b].im * frame[b].im) /
                       (double)nfft;
            double db = 20.0 * log10(m + 1e-12);
            double u = (db + 80.0) / 80.0;
            if (u < 0) u = 0;
            if (u > 1) u = 1;
            buf[row * w + col] = (uint8_t)lrint(u * 255.0);
        }
    }
    free(win); free(frame);
}
