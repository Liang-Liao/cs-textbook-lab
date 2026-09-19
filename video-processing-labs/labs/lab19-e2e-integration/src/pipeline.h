#ifndef LAB19_PIPELINE_H
#define LAB19_PIPELINE_H
#include <pthread.h>
#include <stdint.h>

typedef struct {
    double *data;
    int len;
    int64_t t_ms;
    int seq;
} frame;

typedef struct {
    frame *slots;
    int cap, head, tail, count;
    int dropped;
    int max_seen;
    int done; /* set by producer side when finished */
    pthread_mutex_t mu;
    pthread_cond_t cv;
    int bounded;
    int hard_cap;
} queue;

void q_init(queue *q, int cap, int bounded);
void q_free(queue *q);
int q_push(queue *q, frame f);
void q_finish(queue *q); /* signal end-of-stream; never dropped */
int q_pop(queue *q, frame *f); /* returns -1 when finished and empty */

/* Per-stage probe: energy / peak / clipped count */
typedef struct {
    const char *name;
    double energy;
    double peak;
    int clipped;
    int frames;
    int samples;
} probe_stat;

void probe_reset(probe_stat *p, const char *name);
void probe_accum(probe_stat *p, const double *x, int n);

/* Localize first stage whose peak clips or energy jumps vs previous. */
const char *probe_localize(const probe_stat *stages, int n, double energy_jump_thr,
                           int clip_thr);

#endif
