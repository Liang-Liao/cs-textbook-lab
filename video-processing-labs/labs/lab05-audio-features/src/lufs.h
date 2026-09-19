#ifndef LAB05_LUFS_H
#define LAB05_LUFS_H

/* Simplified BS.1770 / EBU R128 loudness.
 * K-weighting = high-shelf (+4 dB ~1.5 kHz) + RLB high-pass (~38 Hz).
 * Gating: 400 ms blocks, 75% overlap; abs gate -70 LUFS; rel gate -10 LU. */

typedef struct {
    double b0, b1, b2, a1, a2; /* a0 = 1 */
} bq;

void k_weighting_design(bq *shelf, bq *hpf, double fs);
void bq_apply(const bq *c, const double *x, int n, double *y);

/* Integrated loudness (LUFS). Returns -120 if nothing passes gates. */
double lufs_integrated(const double *x, int n, double fs);

/* Same after multiplying samples by `gain` (linear). Convenience wrapper. */
double lufs_integrated_gain(const double *x, int n, double fs, double gain);

#endif
