#ifndef LAB16_PSYCHO_H
#define LAB16_PSYCHO_H

#define PSY_MAX_BANDS 40
#define PSY_MAX_BINS  512

/* Bark-band grouping + spreading + SMR (simplified psychoacoustic model). */
typedef struct {
    int n_bins;
    int n_bands;
    int bin2band[PSY_MAX_BINS];
    int band_start[PSY_MAX_BANDS];
    int band_end[PSY_MAX_BANDS];
    double band_z[PSY_MAX_BANDS];
} bark_map;

double hz_to_bark(double f);
void bark_map_init(bark_map *bm, int n_bins, double fs);

/* power[n_bins]: linear energy per spectral bin.
 * Fills band_E / band_thr / band_smr[0..n_bands). All finite, thr > 0. */
void psycho_analyze_bins(const bark_map *bm, const double *power, double *band_E,
                         double *band_thr, double *band_smr);

/* STFT of one length-n frame (Hann) → power → same outputs. n_bands out. */
int psycho_analyze_frame(const double *x, int n, double fs, double *band_E,
                         double *band_thr, double *band_smr, bark_map *bm_out);

/* Per-bin masking threshold (linear energy), thr_bin[k] = band_thr[b]/nb. */
void psycho_expand_bin_thr(const bark_map *bm, const double *band_thr, double *thr_bin);

#endif
