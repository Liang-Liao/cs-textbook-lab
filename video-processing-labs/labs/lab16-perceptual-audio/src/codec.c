#include "codec.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "adpcm.h"
#include "fft.h"
#include "huffman.h"
#include "mdct.h"
#include "pgm_io.h"
#include "psycho.h"

#define MAX_BITS 8
#define MIN_BITS 2

static void sine_window(double *w, int N) {
    for (int i = 0; i < N; i++)
        w[i] = sin(M_PI / N * (i + 0.5));
}

static double signal_snr_db(const double *ref, const double *dec, int n) {
    double ps = 0, pe = 0;
    for (int i = 0; i < n; i++) {
        double e = ref[i] - dec[i];
        ps += ref[i] * ref[i];
        pe += e * e;
    }
    if (pe <= 1e-30) return 200.0;
    if (ps <= 1e-30) return -200.0;
    return 10.0 * log10(ps / pe);
}

static void stft_power(const double *x, int n, cpx *X, double *power) {
    double *w = malloc((size_t)n * sizeof(double));
    if (!w) return;
    for (int i = 0; i < n; i++) w[i] = 0.5 - 0.5 * cos(2.0 * M_PI * i / n);
    for (int i = 0; i < n; i++) {
        X[i].re = x[i] * w[i];
        X[i].im = 0.0;
    }
    fft(X, n);
    for (int k = 0; k < n; k++)
        power[k] = X[k].re * X[k].re + X[k].im * X[k].im;
    free(w);
}

static int alloc_bits_for_band(double peak, double thr, int nb) {
    if (peak < 1e-20 || nb < 1) return 0;
    double delta_target = sqrt(12.0 * thr / (double)nb);
    if (delta_target < 1e-20) return MAX_BITS;
    double ratio = peak / delta_target;
    if (ratio <= 1.0) return 0;
    int bits = (int)ceil(log2(ratio)) + 1;
    if (bits < MIN_BITS) bits = MIN_BITS;
    if (bits > MAX_BITS) bits = MAX_BITS;
    return bits;
}

/* Quantize from original X into q / Xh. Scales written per band. */
static void quantize_frame(const double *X, int n_bins, const bark_map *bm,
                           const double *band_thr, int *band_bits, double *band_scale,
                           int *q, double *Xh) {
    int nb = bm->n_bands;
    for (int b = 0; b < nb; b++) {
        int lo = bm->band_start[b];
        int hi = bm->band_end[b];
        band_bits[b] = 0;
        band_scale[b] = 0.0;
        if (hi <= lo) continue;
        double peak = 0.0;
        for (int k = lo; k < hi; k++) {
            double a = fabs(X[k]);
            if (a > peak) peak = a;
        }
        int nbins = hi - lo;
        int bits = alloc_bits_for_band(peak, band_thr[b], nbins);
        band_bits[b] = bits;
        if (bits <= 0) {
            for (int k = lo; k < hi; k++) {
                q[k] = 0;
                Xh[k] = 0.0;
            }
            continue;
        }
        int half = 1 << (bits - 1);
        double scale = peak / (double)(half > 1 ? half - 1 : 1);
        if (bits == 1) scale = peak;
        band_scale[b] = scale;
        for (int k = lo; k < hi; k++) {
            int v = (int)lrint(X[k] / scale);
            int qmin = -half, qmax = half - 1;
            if (v < qmin) v = qmin;
            if (v > qmax) v = qmax;
            q[k] = v;
            Xh[k] = (double)v * scale;
        }
    }
    /* bins that fell outside any band range */
    for (int k = 0; k < n_bins; k++) {
        if (q[k] == 0 && Xh[k] == 0.0) {
            /* may be legitimately zero; leave */
        }
    }
}

/* Pack side info + Huffman payload; returns total bits. */
static long pack_frame(const int *band_bits, const double *band_scale, int n_bands,
                       const int *q, int n_bins, uint8_t *buf, int buf_cap) {
    int hdr = 1 + n_bands * 5; /* n_bands u8 + (u8 bits + f32 scale) * nb */
    if (buf_cap < hdr + 16) return -1;
    buf[0] = (uint8_t)n_bands;
    for (int b = 0; b < n_bands; b++) {
        buf[1 + b * 5] = (uint8_t)band_bits[b];
        memcpy(buf + 1 + b * 5 + 1, &band_scale[b], 4);
    }
    int all0 = 1;
    for (int k = 0; k < n_bins; k++) {
        if (q[k] != 0) { all0 = 0; break; }
    }
    if (all0) {
        buf[hdr] = 0;
        return (long)hdr * 8;
    }
    buf[hdr] = 1;
    int hbits = 0;
    int hb = huffman_encode(q, n_bins, buf + hdr + 1, buf_cap - hdr - 1, &hbits);
    if (hb < 0) return -1;
    return (long)(hdr + 1 + hb) * 8;
}

static int unpack_frame(const uint8_t *buf, int buf_bytes, int *band_bits,
                        double *band_scale, int *n_bands, int *q, int n_bins) {
    if (buf_bytes < 1) return -1;
    int nb = buf[0];
    if (nb < 0 || nb > PSY_MAX_BANDS) return -1;
    *n_bands = nb;
    int hdr = 1 + nb * 5;
    if (buf_bytes < hdr + 1) return -1;
    for (int b = 0; b < nb; b++) {
        band_bits[b] = buf[1 + b * 5];
        memcpy(&band_scale[b], buf + 1 + b * 5 + 1, 4);
    }
    if (buf[hdr] == 0) {
        memset(q, 0, (size_t)n_bins * sizeof(int));
        return 0;
    }
    int got = huffman_decode(buf + hdr + 1, buf_bytes - hdr - 1, q, n_bins);
    return got == n_bins ? 0 : -1;
}

int write_noise_pgm(const char *path, const double *spec_db, const double *thr_db,
                    const double *noise_db, int n_frames, int n_bins) {
    if (!path || !spec_db || !thr_db || !noise_db || n_frames < 1 || n_bins < 1)
        return -1;
    int w = 640, h = 256;
    if (n_frames > w) w = n_frames;
    if (n_bins * 2 > h) h = n_bins * 2;
    uint8_t *img = calloc((size_t)w * (size_t)h, 1);
    if (!img) return -1;
    const double db_lo = -100.0, db_hi = 0.0;
    for (int r = 0; r < h; r++) {
        int k = (int)((long long)(h - 1 - r) * (n_bins - 1) / (h > 1 ? h - 1 : 1));
        if (k < 0) k = 0;
        if (k >= n_bins) k = n_bins - 1;
        for (int c = 0; c < w; c++) {
            int t = (int)((long long)c * (n_frames - 1) / (w > 1 ? w - 1 : 1));
            if (t < 0) t = 0;
            if (t >= n_frames) t = n_frames - 1;
            double v = spec_db[(size_t)t * n_bins + k];
            double u = (v - db_lo) / (db_hi - db_lo);
            if (u < 0) u = 0;
            if (u > 1) u = 1;
            img[r * w + c] = (uint8_t)lrint(u * 180.0);
        }
    }
    /* overlay threshold (white) and noise (light gray) curves */
    for (int c = 0; c < w; c++) {
        int t = (int)((long long)c * (n_frames - 1) / (w > 1 ? w - 1 : 1));
        if (t < 0) t = 0;
        if (t >= n_frames) t = n_frames - 1;
        for (int k = 0; k < n_bins; k++) {
            int r = h - 1 - (int)((long long)k * (h - 1) / (n_bins > 1 ? n_bins - 1 : 1));
            if (r < 0 || r >= h) continue;
            double vt = thr_db[(size_t)t * n_bins + k];
            double ut = (vt - db_lo) / (db_hi - db_lo);
            if (ut < 0) ut = 0;
            if (ut > 1) ut = 1;
            int rt = h - 1 - (int)lrint(ut * (h - 1));
            if (rt >= 0 && rt < h) img[rt * w + c] = 255;
            double vn = noise_db[(size_t)t * n_bins + k];
            double un = (vn - db_lo) / (db_hi - db_lo);
            if (un < 0) un = 0;
            if (un > 1) un = 1;
            int rn = h - 1 - (int)lrint(un * (h - 1));
            if (rn >= 0 && rn < h && img[rn * w + c] < 200) img[rn * w + c] = 200;
        }
    }
    int rc = pgm_write(path, img, w, h);
    free(img);
    return rc;
}

double perceptual_process(const double *sig, int n, int N, double fs, double *recon,
                          double *out_snr_db, double *out_bits_per_sample,
                          int *out_n_checked, int collect, double **out_spec,
                          double **out_thr, double **out_noise, int *out_n_frames) {
    if (!sig || n < N || N < 32 || (N & 1)) return 0.0;
    int hop = N / 2;
    int n_bins = N / 2;
    int nframes = 1 + (n - N) / hop;
    double *w = malloc((size_t)N * sizeof(double));
    double *xw = malloc((size_t)N * sizeof(double));
    double *X = malloc((size_t)n_bins * sizeof(double));
    double *Xh = malloc((size_t)n_bins * sizeof(double));
    double *y = calloc((size_t)N, sizeof(double));
    double *out = recon ? recon : calloc((size_t)n, sizeof(double));
    int *q = calloc((size_t)n_bins, sizeof(int));
    double *power = calloc((size_t)n_bins, sizeof(double));
    double *band_E = calloc(PSY_MAX_BANDS, sizeof(double));
    double *band_thr = calloc(PSY_MAX_BANDS, sizeof(double));
    double *band_smr = calloc(PSY_MAX_BANDS, sizeof(double));
    double *band_scale = calloc(PSY_MAX_BANDS, sizeof(double));
    int *band_bits = calloc(PSY_MAX_BANDS, sizeof(int));
    uint8_t *buf = malloc(1 << 16);
    int *q_dec = calloc((size_t)n_bins, sizeof(int));
    double *spec_db = NULL, *thr_db = NULL, *noise_db = NULL;
    bark_map bm;
    if (!w || !xw || !X || !Xh || !y || !out || !q || !power || !band_E || !band_thr ||
        !band_smr || !band_scale || !band_bits || !buf || !q_dec) {
        goto fail;
    }
    bark_map_init(&bm, n_bins, fs);
    if (collect) {
        spec_db = calloc((size_t)nframes * n_bins, sizeof(double));
        thr_db = calloc((size_t)nframes * n_bins, sizeof(double));
        noise_db = calloc((size_t)nframes * n_bins, sizeof(double));
        if (!spec_db || !thr_db || !noise_db) goto fail;
    }
    sine_window(w, N);
    memset(out, 0, (size_t)n * sizeof(double));
    long total_bits = 0;
    int bands_checked = 0, bands_under = 0;

    for (int f = 0; f < nframes; f++) {
        int s0 = f * hop;
        for (int i = 0; i < N; i++) {
            int idx = s0 + i;
            xw[i] = ((idx >= 0 && idx < n) ? sig[idx] : 0.0) * w[i];
        }
        mdct(xw, X, N);
        for (int k = 0; k < n_bins; k++) power[k] = X[k] * X[k];
        psycho_analyze_bins(&bm, power, band_E, band_thr, band_smr);
        quantize_frame(X, n_bins, &bm, band_thr, band_bits, band_scale, q, Xh);

        long fb = pack_frame(band_bits, band_scale, bm.n_bands, q, n_bins, buf, 1 << 16);
        if (fb < 0) goto fail;
        total_bits += fb;

        /* closed-loop: unpack + dequant */
        int nb_dec = 0;
        if (unpack_frame(buf, (int)(fb / 8), band_bits, band_scale, &nb_dec, q_dec,
                         n_bins) != 0)
            goto fail;
        for (int b = 0; b < bm.n_bands; b++) {
            int lo = bm.band_start[b];
            int hi = bm.band_end[b];
            double sc = band_scale[b];
            for (int k = lo; k < hi; k++)
                Xh[k] = (double)q_dec[k] * sc;
        }
        imdct(Xh, y, N);
        for (int i = 0; i < N; i++) {
            int idx = s0 + i;
            if (idx >= 0 && idx < n) out[idx] += y[i] * w[i];
        }

        for (int b = 0; b < bm.n_bands; b++) {
            int lo = bm.band_start[b];
            int hi = bm.band_end[b];
            if (hi <= lo) continue;
            double ne = 0.0;
            for (int k = lo; k < hi; k++) {
                double d = X[k] - Xh[k];
                ne += d * d;
            }
            bands_checked++;
            if (ne <= band_thr[b]) bands_under++;
        }

        if (collect) {
            for (int k = 0; k < n_bins; k++) {
                double e = X[k] * X[k];
                double nz = (X[k] - Xh[k]) * (X[k] - Xh[k]);
                int b = bm.bin2band[k];
                int nb = bm.band_end[b] - bm.band_start[b];
                if (nb < 1) nb = 1;
                double tb = band_thr[b] / (double)nb;
                spec_db[(size_t)f * n_bins + k] = 10.0 * log10(e + 1e-20);
                thr_db[(size_t)f * n_bins + k] = 10.0 * log10(tb + 1e-20);
                noise_db[(size_t)f * n_bins + k] = 10.0 * log10(nz + 1e-20);
            }
        }
    }

    double frac = bands_checked > 0 ? (double)bands_under / bands_checked : 0.0;
    if (out_snr_db) *out_snr_db = signal_snr_db(sig, out, n);
    if (out_bits_per_sample)
        *out_bits_per_sample = (double)total_bits / (double)n;
    if (out_n_checked) *out_n_checked = bands_checked;
    if (collect) {
        if (out_spec) *out_spec = spec_db;
        if (out_thr) *out_thr = thr_db;
        if (out_noise) *out_noise = noise_db;
        if (out_n_frames) *out_n_frames = nframes;
    }
    if (!recon) free(out);
    free(w); free(xw); free(X); free(Xh); free(y); free(q); free(power);
    free(band_E); free(band_thr); free(band_smr); free(band_scale);
    free(band_bits); free(buf); free(q_dec);
    return frac;

fail:
    if (!recon) free(out);
    free(w); free(xw); free(X); free(Xh); free(y); free(q); free(power);
    free(band_E); free(band_thr); free(band_smr); free(band_scale);
    free(band_bits); free(buf); free(q_dec);
    free(spec_db); free(thr_db); free(noise_db);
    return 0.0;
}

double uniform_mdct_snr(const double *sig, int n, int N, double fs, int bits,
                        double *recon, double *out_bits_per_sample) {
    (void)fs;
    if (!sig || n < N || bits < 1) return -200.0;
    int hop = N / 2;
    int n_bins = N / 2;
    int nframes = 1 + (n - N) / hop;
    double *w = malloc((size_t)N * sizeof(double));
    double *xw = malloc((size_t)N * sizeof(double));
    double *X = malloc((size_t)n_bins * sizeof(double));
    double *y = calloc((size_t)N, sizeof(double));
    double *out = recon ? recon : calloc((size_t)n, sizeof(double));
    if (!w || !xw || !X || !y || !out) {
        if (!recon) free(out);
        free(w); free(xw); free(X); free(y);
        return -200.0;
    }
    sine_window(w, N);
    memset(out, 0, (size_t)n * sizeof(double));
    int half = 1 << (bits - 1);
    double gmax = (double)(half > 1 ? half - 1 : 1);
    for (int f = 0; f < nframes; f++) {
        int s0 = f * hop;
        for (int i = 0; i < N; i++) {
            int idx = s0 + i;
            xw[i] = ((idx >= 0 && idx < n) ? sig[idx] : 0.0) * w[i];
        }
        mdct(xw, X, N);
        double peak = 0.0;
        for (int k = 0; k < n_bins; k++)
            if (fabs(X[k]) > peak) peak = fabs(X[k]);
        double scale = peak > 0 ? peak / gmax : 1.0;
        for (int k = 0; k < n_bins; k++) {
            int v = (int)lrint(X[k] / scale);
            if (v < -half) v = -half;
            if (v > half - 1) v = half - 1;
            X[k] = (double)v * scale;
        }
        imdct(X, y, N);
        for (int i = 0; i < N; i++) {
            int idx = s0 + i;
            if (idx >= 0 && idx < n) out[idx] += y[i] * w[i];
        }
    }
    double snr = signal_snr_db(sig, out, n);
    if (out_bits_per_sample)
        *out_bits_per_sample = (double)bits * (double)nframes * n_bins / (double)n;
    if (!recon) free(out);
    free(w); free(xw); free(X); free(y);
    return snr;
}

/* Shared STFT-band noise fraction helper. */
static double stft_band_noise_frac(const double *ref, const double *err, int n,
                                   double fs, int nfft) {
    if (!ref || !err || n < nfft) return 0.0;
    int hop = nfft / 2;
    int nframes = 1 + (n - nfft) / hop;
    if (nframes < 1) return 0.0;
    bark_map bm;
    bark_map_init(&bm, nfft, fs);
    cpx *X = malloc((size_t)nfft * sizeof(cpx));
    double *pr = calloc((size_t)nfft, sizeof(double));
    double *pe = calloc((size_t)nfft, sizeof(double));
    double *band_E = calloc(PSY_MAX_BANDS, sizeof(double));
    double *band_thr = calloc(PSY_MAX_BANDS, sizeof(double));
    double *band_smr = calloc(PSY_MAX_BANDS, sizeof(double));
    double *err_E = calloc(PSY_MAX_BANDS, sizeof(double));
    double *acc_err = calloc(PSY_MAX_BANDS, sizeof(double));
    double *acc_thr = calloc(PSY_MAX_BANDS, sizeof(double));
    if (!X || !pr || !pe || !band_E || !band_thr || !band_smr || !err_E || !acc_err ||
        !acc_thr) {
        free(X); free(pr); free(pe); free(band_E); free(band_thr); free(band_smr);
        free(err_E); free(acc_err); free(acc_thr);
        return 0.0;
    }
    for (int f = 0; f < nframes; f++) {
        int s0 = f * hop;
        stft_power(ref + s0, nfft, X, pr);
        psycho_analyze_bins(&bm, pr, band_E, band_thr, band_smr);
        stft_power(err + s0, nfft, X, pe);
        memset(err_E, 0, PSY_MAX_BANDS * sizeof(double));
        for (int k = 0; k < nfft; k++) err_E[bm.bin2band[k]] += pe[k];
        for (int b = 0; b < bm.n_bands; b++) {
            acc_err[b] += err_E[b];
            acc_thr[b] += band_thr[b];
        }
    }
    int checked = 0, under = 0;
    for (int b = 0; b < bm.n_bands; b++) {
        int lo = bm.band_start[b], hi = bm.band_end[b];
        if (hi <= lo) continue;
        checked++;
        if (acc_err[b] <= acc_thr[b]) under++;
    }
    double frac = checked ? (double)under / checked : 0.0;
    free(X); free(pr); free(pe); free(band_E); free(band_thr); free(band_smr);
    free(err_E); free(acc_err); free(acc_thr);
    return frac;
}

double adpcm_noise_frac(const double *sig, int n, double fs, double *out_snr_db) {
    if (!sig || n < 16) return 0.0;
    int16_t *pcm = malloc((size_t)n * sizeof(int16_t));
    int16_t *dec = malloc((size_t)n * sizeof(int16_t));
    uint8_t *code = malloc((size_t)((n + 1) / 2));
    double *rec = malloc((size_t)n * sizeof(double));
    double *err = malloc((size_t)n * sizeof(double));
    if (!pcm || !dec || !code || !rec || !err) {
        free(pcm); free(dec); free(code); free(rec); free(err);
        return 0.0;
    }
    for (int i = 0; i < n; i++) {
        double v = sig[i] * 30000.0;
        if (v > 32767) v = 32767;
        if (v < -32768) v = -32768;
        pcm[i] = (int16_t)lrint(v);
    }
    adpcm_encode(pcm, n, code);
    adpcm_decode(code, n, dec);
    for (int i = 0; i < n; i++) {
        rec[i] = (double)dec[i] / 30000.0;
        err[i] = sig[i] - rec[i];
    }
    if (out_snr_db) *out_snr_db = signal_snr_db(sig, rec, n);
    int nfft = 256;
    if (n < nfft) nfft = 64;
    double frac = stft_band_noise_frac(sig, err, n, fs, nfft);
    free(pcm); free(dec); free(code); free(rec); free(err);
    return frac;
}

double uniform_noise_frac(const double *sig, int n, int N, double fs, int bits,
                          double *out_snr_db) {
    double *rec = calloc((size_t)n, sizeof(double));
    double *err = malloc((size_t)n * sizeof(double));
    if (!rec || !err) {
        free(rec); free(err);
        return 0.0;
    }
    double snr = uniform_mdct_snr(sig, n, N, fs, bits, rec, NULL);
    if (out_snr_db) *out_snr_db = snr;
    for (int i = 0; i < n; i++) err[i] = sig[i] - rec[i];
    int nfft = N < 256 ? N : 256;
    double frac = stft_band_noise_frac(sig, err, n, fs, nfft);
    free(rec); free(err);
    return frac;
}
