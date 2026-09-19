#include "channel_lite.h"
#include <stdlib.h>
#include <string.h>

/* lab18 relay (min-heap delay + loss + reorder), media-time in-process trim */

static unsigned ch_rng(channel *c) {
    c->rng = c->rng * 1103515245u + 12345u;
    return (c->rng >> 16) & 0x7fff;
}

static int ch_less(const ch_item *x, const ch_item *y) {
    return x->due_ms < y->due_ms;
}

void ch_init(channel *c, int cap, double loss_rate, int max_delay_ms,
             int reorder_pct, unsigned seed) {
    c->cap = cap;
    c->n = 0;
    c->a = calloc((size_t)cap, sizeof(ch_item));
    c->loss_rate = loss_rate;
    c->max_delay_ms = max_delay_ms;
    c->reorder_pct = reorder_pct;
    c->rng = seed ? seed : 1;
    c->sent = c->dropped = c->delivered = 0;
}

void ch_free(channel *c) {
    for (int i = 0; i < c->n; i++) free(c->a[i].data);
    free(c->a);
    c->a = NULL;
    c->n = 0;
}

static void ch_push(channel *c, ch_item it) {
    if (c->n >= c->cap) return;
    int i = c->n++;
    c->a[i] = it;
    while (i > 0) {
        int p = (i - 1) / 2;
        if (!ch_less(&c->a[i], &c->a[p])) break;
        ch_item t = c->a[i];
        c->a[i] = c->a[p];
        c->a[p] = t;
        i = p;
    }
}

static int ch_pop(channel *c, ch_item *out) {
    if (c->n <= 0) return -1;
    *out = c->a[0];
    c->a[0] = c->a[--c->n];
    int i = 0;
    for (;;) {
        int l = 2 * i + 1, r = l + 1, m = i;
        if (l < c->n && ch_less(&c->a[l], &c->a[m])) m = l;
        if (r < c->n && ch_less(&c->a[r], &c->a[m])) m = r;
        if (m == i) break;
        ch_item t = c->a[i];
        c->a[i] = c->a[m];
        c->a[m] = t;
        i = m;
    }
    return 0;
}

int ch_send(channel *c, int now_ms, int seq, const uint8_t *data, int len) {
    c->sent++;
    double u = (ch_rng(c) % 1000) / 1000.0;
    if (u < c->loss_rate) {
        c->dropped++;
        return -1;
    }
    int d = (int)(ch_rng(c) % (c->max_delay_ms + 1));
    if ((int)(ch_rng(c) % 100) < c->reorder_pct)
        d += c->max_delay_ms / 2 + 1; /* push later so order flips */
    ch_item it;
    it.due_ms = now_ms + d;
    it.seq = seq;
    it.len = len;
    it.data = malloc((size_t)(len > 0 ? len : 1));
    if (!it.data) return -1;
    memcpy(it.data, data, (size_t)len);
    ch_push(c, it);
    return 0;
}

int ch_recv(channel *c, int now_ms, int *seq, uint8_t *buf, int cap, int *len) {
    if (c->n <= 0 || c->a[0].due_ms > now_ms) return -1;
    ch_item it;
    if (ch_pop(c, &it) != 0) return -1;
    int n = it.len < cap ? it.len : cap;
    memcpy(buf, it.data, (size_t)n);
    free(it.data);
    if (seq) *seq = it.seq;
    if (len) *len = it.len;
    c->delivered++;
    return 0;
}
