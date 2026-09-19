#ifndef LAB09_AGC_H
#define LAB09_AGC_H
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Static compressor with soft knee. ratio>=1, thresh_db typically negative. */
typedef struct {
    double thresh_db; /* e.g. -18 */
    double ratio;     /* e.g. 4 */
    double knee_db;   /* soft knee width, 0=hard */
    double makeup_db;
} compressor;

double compressor_gain_db(const compressor *c, double level_db);
/* x scaled by the static gain for the caller-tracked peak envelope (*env). */
double compressor_run_sample(const compressor *c, double x, double *env);

/* Lookahead soft limiter: the gain is derived from the UNDELAYED envelope
 * (it sees peaks before they reach the output) and smoothed, so peaks are
 * shaped down to `limit` and the brick-wall safety clamp never engages. */
typedef struct {
    double limit;        /* target output peak, e.g. 0.95 */
    double att_coef;     /* gain reduction speed */
    double rel_coef;     /* gain recovery speed */
    double env_rel_coef; /* envelope follower release */
    double gain;
    double env;
    double *dl;          /* lookahead delay line */
    int dlen, dpos;
} limiter;

void limiter_init(limiter *l, double limit, double fs, double att_ms,
                  double rel_ms, double lookahead_ms);
void limiter_free(limiter *l);
double limiter_process(limiter *l, double x);

/* AGC: target peak envelope in dBFS, attack/release on gain. */
typedef struct {
    double target_db;
    double att_coef;
    double rel_coef;
    double max_gain_db;
    double min_gain_db;
    double gain;
    double env;
    double peak_limit;
} agc;

void agc_init(agc *a, double target_db, double fs, double att_s, double rel_s);
double agc_process(agc *a, double x);

double rms_db(const double *x, int n);
double peak_abs(const double *x, int n);

#endif
