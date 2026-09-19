#include "vad.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void frame_energy_db(const double *x, int n, int frame, int hop, double *out,
                     int *n_frames) {
    if (!x || !out || frame <= 0 || hop <= 0) {
        if (n_frames) *n_frames = 0;
        return;
    }
    int nf = 1 + (n - frame) / hop;
    if (nf < 1) nf = 1;
    for (int f = 0; f < nf; f++) {
        int start = f * hop;
        double acc = 0.0;
        for (int i = 0; i < frame; i++) {
            int idx = start + i;
            double v = (idx >= 0 && idx < n) ? x[idx] : 0.0;
            acc += v * v;
        }
        out[f] = 10.0 * log10(acc / (double)frame + 1e-12);
    }
    if (n_frames) *n_frames = nf;
}

void frame_zcr(const double *x, int n, int frame, int hop, double *out,
               int *n_frames) {
    if (!x || !out || frame <= 0 || hop <= 0) {
        if (n_frames) *n_frames = 0;
        return;
    }
    int nf = 1 + (n - frame) / hop;
    if (nf < 1) nf = 1;
    for (int f = 0; f < nf; f++) {
        int start = f * hop;
        int zc = 0;
        double prev = (start >= 0 && start < n) ? x[start] : 0.0;
        for (int i = 1; i < frame; i++) {
            int idx = start + i;
            double cur = (idx >= 0 && idx < n) ? x[idx] : 0.0;
            if ((prev >= 0.0 && cur < 0.0) || (prev < 0.0 && cur >= 0.0)) zc++;
            prev = cur;
        }
        out[f] = (double)zc / (double)frame;
    }
    if (n_frames) *n_frames = nf;
}

void vad_dual_threshold(const double *energy_db, const double *zcr, int n_frames,
                        double energy_hi, double energy_lo, double zcr_hi,
                        int hangover, unsigned char *mask) {
    if (!energy_db || !mask || n_frames <= 0) return;
    int hyst = 0;
    for (int i = 0; i < n_frames; i++) {
        int speech = 0;
        if (energy_db[i] >= energy_hi) {
            speech = 1;
            hyst = hangover;
        } else if (energy_db[i] >= energy_lo) {
            /* mid band: use hangover if recently speech, else ZCR (unvoiced vs noise) */
            if (hyst > 0) {
                speech = 1;
                hyst--;
            } else if (zcr && zcr[i] >= zcr_hi) {
                /* high ZCR + mid energy → treat as unvoiced speech candidate */
                speech = 1;
                hyst = hangover / 2;
            } else {
                speech = 0;
            }
        } else {
            if (hyst > 0) {
                speech = 1;
                hyst--;
            } else {
                speech = 0;
            }
        }
        mask[i] = (unsigned char)speech;
    }
}

void gmm2_em(const double *x, int n, int iters, gmm2 *g) {
    if (!x || !g || n < 2) return;
    /* init: min/max split (adequate for bimodal log-energy) */
    double lo = x[0], hi = x[0];
    for (int i = 1; i < n; i++) {
        if (x[i] < lo) lo = x[i];
        if (x[i] > hi) hi = x[i];
    }
    g->mu[0] = lo + 0.25 * (hi - lo);
    g->mu[1] = lo + 0.75 * (hi - lo);
    g->var[0] = 4.0;
    g->var[1] = 4.0;
    g->w[0] = 0.5;
    g->w[1] = 0.5;

    double *r0 = malloc((size_t)n * sizeof(double));
    double *r1 = malloc((size_t)n * sizeof(double));
    if (!r0 || !r1) {
        free(r0);
        free(r1);
        return;
    }
    for (int it = 0; it < iters; it++) {
        /* E */
        for (int i = 0; i < n; i++) {
            double d0 = x[i] - g->mu[0];
            double d1 = x[i] - g->mu[1];
            double p0 = g->w[0] * exp(-0.5 * d0 * d0 / (g->var[0] + 1e-12)) /
                        sqrt(2.0 * M_PI * (g->var[0] + 1e-12));
            double p1 = g->w[1] * exp(-0.5 * d1 * d1 / (g->var[1] + 1e-12)) /
                        sqrt(2.0 * M_PI * (g->var[1] + 1e-12));
            double s = p0 + p1 + 1e-300;
            r0[i] = p0 / s;
            r1[i] = p1 / s;
        }
        /* M */
        double n0 = 0.0, n1 = 0.0, s0 = 0.0, s1 = 0.0, q0 = 0.0, q1 = 0.0;
        for (int i = 0; i < n; i++) {
            n0 += r0[i];
            n1 += r1[i];
            s0 += r0[i] * x[i];
            s1 += r1[i] * x[i];
        }
        g->mu[0] = s0 / (n0 + 1e-12);
        g->mu[1] = s1 / (n1 + 1e-12);
        for (int i = 0; i < n; i++) {
            double d0 = x[i] - g->mu[0];
            double d1 = x[i] - g->mu[1];
            q0 += r0[i] * d0 * d0;
            q1 += r1[i] * d1 * d1;
        }
        g->var[0] = q0 / (n0 + 1e-12);
        g->var[1] = q1 / (n1 + 1e-12);
        if (g->var[0] < 1e-6) g->var[0] = 1e-6;
        if (g->var[1] < 1e-6) g->var[1] = 1e-6;
        g->w[0] = n0 / (double)n;
        g->w[1] = n1 / (double)n;
    }
    free(r0);
    free(r1);
}

double gmm2_loglik(const gmm2 *g, double x) {
    double best = -1e300;
    for (int k = 0; k < 2; k++) {
        double d = x - g->mu[k];
        double ll = log(g->w[k] + 1e-300) - 0.5 * log(2.0 * M_PI * (g->var[k] + 1e-12)) -
                    0.5 * d * d / (g->var[k] + 1e-12);
        if (ll > best) best = ll;
    }
    return best;
}

void vad_gmm(const double *energy_db, int n_frames, const gmm2 *g, double thr,
             unsigned char *mask) {
    if (!energy_db || !g || !mask) return;
    /* speech = higher mean energy component */
    int sp = (g->mu[1] > g->mu[0]) ? 1 : 0;
    int ns = 1 - sp;
    for (int i = 0; i < n_frames; i++) {
        double d = energy_db[i];
        double dsp = d - g->mu[sp];
        double dns = d - g->mu[ns];
        double ll_sp = log(g->w[sp] + 1e-300) -
                       0.5 * log(2.0 * M_PI * (g->var[sp] + 1e-12)) -
                       0.5 * dsp * dsp / (g->var[sp] + 1e-12);
        double ll_ns = log(g->w[ns] + 1e-300) -
                       0.5 * log(2.0 * M_PI * (g->var[ns] + 1e-12)) -
                       0.5 * dns * dns / (g->var[ns] + 1e-12);
        mask[i] = (unsigned char)((ll_sp - ll_ns) >= thr ? 1 : 0);
    }
}

int mask_first(const unsigned char *m, int n) {
    for (int i = 0; i < n; i++)
        if (m[i]) return i;
    return -1;
}

int mask_last(const unsigned char *m, int n) {
    for (int i = n - 1; i >= 0; i--)
        if (m[i]) return i;
    return -1;
}

void vad_eval(const unsigned char *pred, const unsigned char *truth, int n_frames,
              int hop, double fs, vad_metrics *m) {
    memset(m, 0, sizeof(*m));
    if (!pred || !truth || n_frames <= 0) return;
    for (int i = 0; i < n_frames; i++) {
        if (pred[i] && truth[i])
            m->tp++;
        else if (pred[i] && !truth[i])
            m->fp++;
        else if (!pred[i] && truth[i])
            m->fn++;
        else
            m->tn++;
    }
    m->precision = (m->tp + m->fp > 0) ? (double)m->tp / (double)(m->tp + m->fp) : 0.0;
    m->recall = (m->tp + m->fn > 0) ? (double)m->tp / (double)(m->tp + m->fn) : 0.0;
    m->f1 = (m->precision + m->recall > 0)
                ? 2.0 * m->precision * m->recall / (m->precision + m->recall)
                : 0.0;
    int pf = mask_first(pred, n_frames);
    int pl = mask_last(pred, n_frames);
    int tf = mask_first(truth, n_frames);
    int tl = mask_last(truth, n_frames);
    double ms_per_hop = 1000.0 * (double)hop / fs;
    if (pf >= 0 && tf >= 0) m->start_err_ms = fabs((double)(pf - tf)) * ms_per_hop;
    else m->start_err_ms = 1e9;
    if (pl >= 0 && tl >= 0) m->end_err_ms = fabs((double)(pl - tl)) * ms_per_hop;
    else m->end_err_ms = 1e9;
}
