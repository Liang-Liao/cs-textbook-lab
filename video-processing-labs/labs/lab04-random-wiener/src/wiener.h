#ifndef LAB04_WIENER_H
#define LAB04_WIENER_H
#include <stddef.h>
/* Frequency-domain Wiener with known Ps,Pn per bin. H = Ps/(Ps+Pn). */
void wiener_gain_from_psd(const double *Ps, const double *Pn, int n, double *H);
void apply_wiener_frame(const double *frame, const double *Ps, const double *Pn,
                        int n, double *out);
double snr_db(const double *ref, const double *test, size_t n);
#endif
