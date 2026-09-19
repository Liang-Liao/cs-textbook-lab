#include "pipeline.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

void q_init(queue *q, int cap, int bounded) {
    q->cap = cap > 0 ? cap : 8;
    q->head = q->tail = q->count = q->dropped = q->max_seen = 0;
    q->done = 0;
    q->bounded = bounded;
    q->hard_cap = bounded ? q->cap : 10000;
    q->slots = calloc((size_t)q->hard_cap, sizeof(frame));
    pthread_mutex_init(&q->mu, NULL);
    pthread_cond_init(&q->cv, NULL);
}

void q_free(queue *q) {
    for (int i = 0; i < q->count; i++) {
        int idx = (q->head + i) % q->hard_cap;
        free(q->slots[idx].data);
    }
    free(q->slots);
    q->slots = NULL;
    pthread_mutex_destroy(&q->mu);
    pthread_cond_destroy(&q->cv);
}

int q_push(queue *q, frame f) {
    pthread_mutex_lock(&q->mu);
    if (q->count >= (q->bounded ? q->cap : q->hard_cap)) {
        q->dropped++;
        pthread_mutex_unlock(&q->mu);
        free(f.data);
        return -1;
    }
    q->slots[q->tail] = f;
    q->tail = (q->tail + 1) % q->hard_cap;
    q->count++;
    if (q->count > q->max_seen) q->max_seen = q->count;
    pthread_cond_signal(&q->cv);
    pthread_mutex_unlock(&q->mu);
    return 0;
}

void q_finish(queue *q) {
    pthread_mutex_lock(&q->mu);
    q->done = 1;
    pthread_cond_broadcast(&q->cv);
    pthread_mutex_unlock(&q->mu);
}

int q_pop(queue *q, frame *f) {
    pthread_mutex_lock(&q->mu);
    while (q->count <= 0) {
        if (q->done) {
            pthread_mutex_unlock(&q->mu);
            return -1;
        }
        pthread_cond_wait(&q->cv, &q->mu);
    }
    *f = q->slots[q->head];
    q->head = (q->head + 1) % q->hard_cap;
    q->count--;
    pthread_mutex_unlock(&q->mu);
    return 0;
}

void probe_reset(probe_stat *p, const char *name) {
    memset(p, 0, sizeof(*p));
    p->name = name;
}

void probe_accum(probe_stat *p, const double *x, int n) {
    for (int i = 0; i < n; i++) {
        double v = x[i];
        p->energy += v * v;
        double a = fabs(v);
        if (a > p->peak) p->peak = a;
        if (a > 0.99) p->clipped++;
        p->samples++;
    }
    p->frames++;
}

const char *probe_localize(const probe_stat *stages, int n, double energy_jump_thr,
                           int clip_thr) {
    for (int i = 0; i < n; i++) {
        if (stages[i].clipped > clip_thr) return stages[i].name;
        if (i > 0) {
            double e0 = stages[i - 1].energy / (stages[i - 1].samples ? stages[i - 1].samples : 1);
            double e1 = stages[i].energy / (stages[i].samples ? stages[i].samples : 1);
            if (e0 > 1e-12 && e1 / e0 > energy_jump_thr) return stages[i].name;
        }
    }
    return NULL;
}
