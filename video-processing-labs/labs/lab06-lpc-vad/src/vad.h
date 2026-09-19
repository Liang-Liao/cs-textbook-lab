#ifndef LAB06_VAD_H
#define LAB06_VAD_H

#include <stddef.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Frame features */
void frame_energy_db(const double *x, int n, int frame, int hop, double *out,
                     int *n_frames);
void frame_zcr(const double *x, int n, int frame, int hop, double *out,
               int *n_frames);

/* Dual-threshold VAD (energy + ZCR hangover).
 * Returns binary speech mask per frame (1=speech). */
void vad_dual_threshold(const double *energy_db, const double *zcr, int n_frames,
                        double energy_hi, double energy_lo, double zcr_hi,
                        int hangover, unsigned char *mask);

/* Simple 2-component GMM + EM on log-energy, likelihood-ratio VAD. */
typedef struct {
    double mu[2], var[2], w[2];
} gmm2;

void gmm2_em(const double *x, int n, int iters, gmm2 *g);
double gmm2_loglik(const gmm2 *g, double x);
void vad_gmm(const double *energy_db, int n_frames, const gmm2 *g, double thr,
             unsigned char *mask);

/* Segment-level metrics vs ground-truth frame mask. */
typedef struct {
    double precision, recall, f1;
    int tp, fp, fn, tn;
    double start_err_ms, end_err_ms; /* absolute first/last speech sample err */
} vad_metrics;

void vad_eval(const unsigned char *pred, const unsigned char *truth, int n_frames,
              int hop, double fs, vad_metrics *m);

/* Find first/last 1-index in mask; -1 if empty. */
int mask_first(const unsigned char *m, int n);
int mask_last(const unsigned char *m, int n);

#endif
