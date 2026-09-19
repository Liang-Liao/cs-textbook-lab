#ifndef LAB19_NS_H
#define LAB19_NS_H
/* tiny time-domain noise gate (NS-lite), not a full spectral NS */
typedef struct {
    double floor_gain;
    double noise_ema;
    double alpha;
} ns_lite;

void ns_lite_init(ns_lite *n, double floor_gain);
double ns_lite_process(ns_lite *n, double x);
#endif
