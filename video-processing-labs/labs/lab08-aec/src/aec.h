#ifndef LAB08_AEC_H
#define LAB08_AEC_H
#include <stddef.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    int L;            /* filter length */
    double *h;        /* adaptive filter */
    double *xbuf;     /* delay line */
    double mu;
} nlms;

void nlms_init(nlms *f, int L, double mu);
void nlms_free(nlms *f);
void nlms_reset(nlms *f);
/* one step: far x, mic d, freeze (skip update) if dt */
double nlms_step(nlms *f, double x, double d, int freeze);

/* Frequency-domain cross-correlation delay estimate (sample index).
 * Returns lag τ such that d[n] ≈ x[n - τ] (d delayed relative to x). */
int estimate_delay(const double *x, const double *d, int n, int max_delay);

double erle_db(const double *echo, const double *err, int n);

/* Exponential-decay random impulse response. */
void synth_path(double *h, int L, double tau, unsigned seed);

/* Simple NLP: residual echo suppression. The AEC residual is compared against
 * its own echo estimate: residual-echo-level error → suppress toward floor_g,
 * much-larger-than-echo error (near-end) → pass through. */
typedef struct {
    double echo_sm;     /* slow smoothed echo-estimate power */
    double err_sm;      /* fast smoothed error power */
    double floor_g;     /* minimum gain (max suppression) */
    double resid_ratio; /* expected residual/echo power ratio when converged */
} nlp;

void nlp_init(nlp *n, double floor_g, double resid_ratio);
/* echo_est: adaptive filter echo estimate (mic - aec error), e: aec error,
 * far_active: far energy gate. Returns suppressed error sample. */
double nlp_step(nlp *n, double echo_est, double e, int far_active);

/* Signal-driven double-talk detector: block energy ratio r = e²/ŷ² between
 * the AEC residual and its own echo estimate, with minimum-statistics
 * calibration — the DT threshold tracks the converged residual floor
 * (min of recent per-block minima), so no ground truth and no hand-set
 * absolute level are needed. A high residual is ambiguous (near-end speech
 * vs a misconverged filter), so after max_stay frozen blocks the detector
 * releases one probe block and judges by the residual trend: falling →
 * the filter was recovering (stay unfrozen), flat → real double talk
 * (re-freeze). */
typedef struct {
    int blk;            /* samples per decision block */
    int hist;           /* per-block minima kept (history depth) */
    double *mins;       /* ring of recent per-block minima of r */
    double acc_e2;      /* within-block pooled residual energy */
    double acc_y2;      /* within-block pooled echo-estimate energy */
    int nacc;
    double beta;        /* tracked residual-to-echo floor */
    double k_ratio;     /* DT when block r exceeds k_ratio·beta (or r_min) */
    double r_min;       /* absolute threshold floor */
    int max_stay;       /* continuous freeze cap before a probe block */
    int pos;
    int dt;             /* current state: 1 = double talk (frozen) */
    int below;          /* consecutive blocks below threshold */
    int stay;           /* consecutive frozen blocks */
    int probing;        /* 1 = released for a recovery probe */
    double r_prev;      /* previous block ratio (probe progress check) */
} dt_det;

void dt_init(dt_det *d, int blk, int hist, double k_ratio, double r_min,
             int max_stay);
void dt_free(dt_det *d);
/* Push one sample (echo estimate ŷ = mic − aec_error, aec residual e);
 * returns 1 while double-talk is detected. */
int dt_step(dt_det *d, double echo_est, double e);

/* Partitioned block frequency-domain adaptive filter (PBFDAF).
 * Overlap-save, P partitions of length N (filter length L = P*N). */
typedef struct {
    int N;      /* FFT/block size (power of 2) */
    int P;      /* partitions */
    double *Xf; /* P*N*2 — partitioned far spectra (re,im interleaved as double pairs) */
    double *H;  /* P * N * 2 filter spectra */
    double *xold;
    double *xf;  /* scratch FFT of current block */
    double *Yf;  /* scratch echo estimate spectrum */
    double *Ef;  /* scratch error spectrum */
    double *Ptot; /* per-bin total input power across partitions */
    double mu;
} pbfdaf;

void pbfdaf_init(pbfdaf *f, int N, int P, double mu);
void pbfdaf_free(pbfdaf *f);
void pbfdaf_reset(pbfdaf *f);
/* Process one block of N samples. far[N], mic[N] → err[N] (OLA-free overlap-save). */
void pbfdaf_process(pbfdaf *f, const double *far, const double *mic, double *err);

#endif
