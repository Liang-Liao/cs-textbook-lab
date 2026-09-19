/* copied from lab05-audio-features */
#ifndef LAB09_LUFS_H
#define LAB09_LUFS_H
/* Simplified integrated loudness (EBU R128-ish): K-weight approx + absolute gate. */
double lufs_integrated(const double *x, int n, double fs);
#endif
