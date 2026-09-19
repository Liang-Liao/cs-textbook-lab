#include "psycho.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "fft.h"

/* ~12 dB SNR offset: mix of tonal / noise-like maskers. */
#define SNR_OFFSET_DB 12.0
/* Absolute hearing floor in linear band-energy units (toy scale). */
#define ATH_FLOOR 1e-12

double hz_to_bark(double f) {
    if (f < 0.0) f = 0.0;
    return 13.0 * atan(0.00076 * f) + 3.5 * atan((f / 7500.0) * (f / 7500.0));
}

void bark_map_init(bark_map *bm, int n_bins, double fs) {
    if (!bm || n_bins <= 0 || n_bins > PSY_MAX_BINS) return;
    memset(bm, 0, sizeof(*bm));
    bm->n_bins = n_bins;
    /* bins cover [0, fs/2): f_k = k * fs / (2 * n_bins) */
    double zmax = hz_to_bark(0.5 * fs);
    if (zmax < 1.0) zmax = 1.0;
    int nb = (int)ceil(zmax);
    if (nb < 8) nb = 8;
    if (nb > PSY_MAX_BANDS) nb = PSY_MAX_BANDS;
    bm->n_bands = nb;
    for (int b = 0; b < nb; b++) {
        bm->band_start[b] = n_bins;
        bm->band_end[b] = 0;
        bm->band_z[b] = (b + 0.5) * zmax / nb;
    }
    for (int k = 0; k < n_bins; k++) {
        double f = (double)k * fs / (2.0 * (double)n_bins);
        double z = hz_to_bark(f);
        int b = (int)(z * (double)nb / zmax);
        if (b < 0) b = 0;
        if (b >= nb) b = nb - 1;
        bm->bin2band[k] = b;
        if (k < bm->band_start[b]) bm->band_start[b] = k;
        if (k + 1 > bm->band_end[b]) bm->band_end[b] = k + 1;
    }
    for (int b = 0; b < nb; b++) {
        if (bm->band_end[b] <= bm->band_start[b]) {
            bm->band_start[b] = 0;
            bm->band_end[b] = 0;
        }
    }
}

/* Asymmetric spreading: flatter toward higher frequencies (~ -10 dB/Bark),
 * steeper toward lower (~ -27 dB/Bark). */
static void apply_spreading(const double *E, const bark_map *bm, double *E_spread) {
    int nb = bm->n_bands;
    for (int i = 0; i < nb; i++) {
        double acc = 0.0;
        for (int j = 0; j < nb; j++) {
            double dz = bm->band_z[i] - bm->band_z[j];
            double g_db = (dz >= 0.0) ? (-10.0 * dz) : (-27.0 * (-dz));
            acc += E[j] * pow(10.0, g_db / 10.0);
        }
        E_spread[i] = acc;
    }
}

void psycho_analyze_bins(const bark_map *bm, const double *power, double *band_E,
                         double *band_thr, double *band_smr) {
    if (!bm || !power || !band_E || !band_thr || !band_smr) return;
    int nb = bm->n_bands;
    memset(band_E, 0, (size_t)nb * sizeof(double));
    for (int k = 0; k < bm->n_bins; k++) {
        band_E[bm->bin2band[k]] += power[k];
    }
    double *spread = malloc((size_t)nb * sizeof(double));
    if (!spread) return;
    apply_spreading(band_E, bm, spread);
    double thr_lin = pow(10.0, -SNR_OFFSET_DB / 10.0);
    for (int b = 0; b < nb; b++) {
        band_thr[b] = spread[b] * thr_lin;
        if (band_thr[b] < ATH_FLOOR) band_thr[b] = ATH_FLOOR;
        band_smr[b] = band_E[b] / band_thr[b];
    }
    free(spread);
}

int psycho_analyze_frame(const double *x, int n, double fs, double *band_E,
                         double *band_thr, double *band_smr, bark_map *bm_out) {
    if (!x || n <= 0 || n > PSY_MAX_BINS || (n & (n - 1)) != 0) return -1;
    int n_bins = n / 2;
    cpx *X = malloc((size_t)n * sizeof(cpx));
    double *w = malloc((size_t)n * sizeof(double));
    double *power = calloc((size_t)n_bins, sizeof(double));
    bark_map bm_local;
    bark_map *bm = bm_out ? bm_out : &bm_local;
    if (!X || !w || !power) {
        free(X); free(w); free(power);
        return -1;
    }
    for (int i = 0; i < n; i++)
        w[i] = 0.5 - 0.5 * cos(2.0 * M_PI * (double)i / (double)n);
    for (int i = 0; i < n; i++) {
        X[i].re = x[i] * w[i];
        X[i].im = 0.0;
    }
    fft(X, n);
    for (int k = 0; k < n_bins; k++)
        power[k] = X[k].re * X[k].re + X[k].im * X[k].im;
    bark_map_init(bm, n_bins, fs);
    psycho_analyze_bins(bm, power, band_E, band_thr, band_smr);
    free(X); free(w); free(power);
    return bm->n_bands;
}

void psycho_expand_bin_thr(const bark_map *bm, const double *band_thr, double *thr_bin) {
    if (!bm || !band_thr || !thr_bin) return;
    for (int k = 0; k < bm->n_bins; k++) {
        int b = bm->bin2band[k];
        int nb = bm->band_end[b] - bm->band_start[b];
        if (nb < 1) nb = 1;
        thr_bin[k] = band_thr[b] / (double)nb;
    }
}
