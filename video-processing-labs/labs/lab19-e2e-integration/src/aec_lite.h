#ifndef LAB19_AEC_LITE_H
#define LAB19_AEC_LITE_H

/* copied from lab08 src/aec.c (nlms + erle), trimmed for the E2E chain:
 * far-reference NLMS echo canceller + ERLE metric. */

typedef struct {
    int L;
    double *h;
    double *xbuf;
    double mu;
} aec_nlms;

void aec_nlms_init(aec_nlms *f, int L, double mu);
void aec_nlms_free(aec_nlms *f);
/* one step: far reference x, mic d; freeze skips the update (near-end) */
double aec_nlms_step(aec_nlms *f, double x, double d, int freeze);

/* ERLE = 10*log10(P_reference / P_residual) over the window */
double aec_erle_db(const double *ref, const double *res, int n);

#endif
