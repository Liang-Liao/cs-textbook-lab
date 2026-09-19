#ifndef LAB19_AGC_H
#define LAB19_AGC_H
/* simplified AGC from lab09 */
typedef struct {
    double target_db;
    double att_coef;
    double rel_coef;
    double gain;
    double env;
    double peak_limit;
    double injected_scale; /* bug injection hook; normally 1.0 */
} agc;

void agc_init(agc *a, double target_db, double fs, double att_s, double rel_s);
double agc_process(agc *a, double x);
#endif
