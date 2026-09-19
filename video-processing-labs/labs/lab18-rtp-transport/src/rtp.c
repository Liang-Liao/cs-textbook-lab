/* lab18 RTP: pack/unpack + adaptive JB + GCC-lite */
#include "rtp.h"
#include <stdlib.h>
#include <string.h>

static void put_be64(uint8_t *p, int64_t v) {
    uint64_t u = (uint64_t)v;
    for (int i = 7; i >= 0; i--) {
        p[i] = (uint8_t)(u & 0xff);
        u >>= 8;
    }
}
static int64_t get_be64(const uint8_t *p) {
    uint64_t u = 0;
    for (int i = 0; i < 8; i++) u = (u << 8) | p[i];
    return (int64_t)u;
}

int rtp_pack(const rtp_pkt *p, uint8_t *buf, int cap) {
    if (!p || !buf || cap < 20 + p->len) return -1;
    buf[0] = 0x80; /* V=2, no padding/extension/CSRC */
    buf[1] = (uint8_t)(((p->marker & 1) << 7) | (p->pt & 0x7f));
    buf[2] = (uint8_t)(p->seq >> 8);
    buf[3] = (uint8_t)(p->seq & 0xff);
    buf[4] = (uint8_t)(p->ts >> 24);
    buf[5] = (uint8_t)(p->ts >> 16);
    buf[6] = (uint8_t)(p->ts >> 8);
    buf[7] = (uint8_t)(p->ts);
    buf[8] = (uint8_t)(p->ssrc >> 24);
    buf[9] = (uint8_t)(p->ssrc >> 16);
    buf[10] = (uint8_t)(p->ssrc >> 8);
    buf[11] = (uint8_t)(p->ssrc);
    put_be64(buf + 12, p->send_ms);
    if (p->len > 0 && p->payload) memcpy(buf + 20, p->payload, (size_t)p->len);
    return 20 + p->len;
}

int rtp_unpack(const uint8_t *buf, int n, rtp_pkt *p) {
    if (!buf || !p || n < 20) return -1;
    if ((buf[0] >> 6) != 2) return -1;
    p->marker = (buf[1] >> 7) & 1;
    p->pt = buf[1] & 0x7f;
    p->seq = (uint16_t)((buf[2] << 8) | buf[3]);
    p->ts = ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) | ((uint32_t)buf[6] << 8) | buf[7];
    p->ssrc = ((uint32_t)buf[8] << 24) | ((uint32_t)buf[9] << 16) |
              ((uint32_t)buf[10] << 8) | buf[11];
    p->send_ms = get_be64(buf + 12);
    p->len = n - 20;
    p->payload = (uint8_t *)buf + 20;
    return 0;
}

void ajb_init(ajb *b, int cap) {
    memset(b, 0, sizeof(*b));
    b->cap = cap > 0 ? cap : 64;
    b->slots = calloc((size_t)b->cap, sizeof(ajb_slot));
    b->min_target_ms = 20;
    b->max_target_ms = 200;
    b->target_ms = 40;
    b->first_recv_ms = -1;
}

void ajb_free(ajb *b) {
    if (!b || !b->slots) return;
    for (int i = 0; i < b->cap; i++) free(b->slots[i].payload);
    free(b->slots);
    b->slots = NULL;
    b->count = 0;
}

int ajb_push(ajb *b, const rtp_pkt *p, int64_t now_ms) {
    if (!b->slots || b->count >= b->cap) return -1;
    if (ajb_has(b, p->seq)) return 0; /* duplicate */
    double delay = (double)(now_ms - p->send_ms);
    if (b->first_recv_ms < 0) {
        b->first_recv_ms = now_ms;
        b->delay_ema = delay;
        b->jitter = delay * 0.25;
    } else {
        double d = delay - b->delay_ema;
        if (d < 0) d = -d;
        b->jitter += (d - b->jitter) / 16.0;
        b->delay_ema += (delay - b->delay_ema) / 32.0;
    }
    int tgt = (int)(b->jitter * 3.0 + 25.0);
    if (tgt < b->min_target_ms) tgt = b->min_target_ms;
    if (tgt > b->max_target_ms) tgt = b->max_target_ms;
    b->target_ms = tgt;

    int idx = -1;
    for (int i = 0; i < b->cap; i++) {
        if (!b->slots[i].used) {
            idx = i;
            break;
        }
    }
    if (idx < 0) return -1;
    ajb_slot *s = &b->slots[idx];
    s->seq = p->seq;
    s->ts = p->ts;
    s->pt = p->pt;
    s->send_ms = p->send_ms;
    s->recv_ms = now_ms;
    s->len = p->len;
    s->payload = malloc((size_t)(p->len > 0 ? p->len : 1));
    if (!s->payload) return -1;
    if (p->len > 0 && p->payload) memcpy(s->payload, p->payload, (size_t)p->len);
    s->used = 1;
    b->count++;
    return 0;
}

int ajb_has(const ajb *b, uint16_t seq) {
    for (int i = 0; i < b->cap; i++)
        if (b->slots[i].used && b->slots[i].seq == seq) return 1;
    return 0;
}

int ajb_pop(ajb *b, uint16_t expect_seq, rtp_pkt *out) {
    for (int i = 0; i < b->cap; i++) {
        if (b->slots[i].used && b->slots[i].seq == expect_seq) {
            ajb_slot *s = &b->slots[i];
            out->seq = s->seq;
            out->ts = s->ts;
            out->pt = s->pt;
            out->send_ms = s->send_ms;
            out->payload = s->payload;
            out->len = s->len;
            s->used = 0;
            s->payload = NULL;
            b->count--;
            return 0;
        }
    }
    return -1;
}

void gcc_init(gcc_t *g) {
    g->trend_ms = 0;
    g->state = 2; /* start increasing toward max */
    g->rate = 0.8;
    g->decrease_count = 0;
    g->increase_count = 0;
    g->loss_events = 0;
}

double gcc_update(gcc_t *g, double grad_ms, double loss_pct) {
    g->trend_ms = 0.9 * g->trend_ms + 0.1 * grad_ms;
    int lossy = loss_pct > 5.0;
    if (g->trend_ms > 4.0 || lossy) {
        if (g->state != 1) g->decrease_count++;
        if (lossy) g->loss_events++;
        g->state = 1;
        /* loss → multiplicative decrease, more aggressive than the
         * gradient's gentle 0.88 back-off */
        g->rate *= lossy ? 0.5 : 0.88;
        if (g->rate < 0.5) g->rate = 0.5;
    } else if (g->trend_ms < 1.0) {
        if (g->state != 2) g->increase_count++;
        g->state = 2;
        g->rate *= 1.03;
        if (g->rate > 1.2) g->rate = 1.2;
    } else {
        g->state = 0;
    }
    return g->rate;
}

/* ---------- audio-master A/V sync ---------- */

void avsync_init(avsync *s, double display_latency_ms, double tol_ms) {
    s->display_latency_ms = display_latency_ms;
    s->tol_ms = tol_ms;
    s->skew_ema_ms = 0.0;
    s->displayed = s->dropped = s->held = 0;
}

int avsync_frame(avsync *s, double video_ms, double audio_master_ms,
                 double *due_in_ms) {
    /* how long until the audio master reaches this frame's display moment */
    double due_in = (video_ms + s->display_latency_ms) - audio_master_ms;
    /* skew estimate: positive → frames tend to arrive after their display
     * moment (video path lags the audio master) */
    double skew = -due_in;
    s->skew_ema_ms += 0.2 * (skew - s->skew_ema_ms);
    if (due_in_ms) *due_in_ms = due_in;
    if (due_in < -s->tol_ms) {
        /* stale: the master already moved past its display slot */
        s->dropped++;
        return 0;
    }
    if (due_in > 1000.0) {
        /* far in the future: hold the previous frame */
        s->held++;
        return -1;
    }
    s->displayed++;
    return 1;
}
