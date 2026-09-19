/* lab18 selftest: UDP loopback + channel sim + adaptive JB + GCC + PLC + A/V sync */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "rtp.h"
#include "udp.h"

static int g_pass, g_fail;
static void report(const char *n, double v, const char *op, double thr, int ok) {
    if (ok) {
        g_pass++;
        printf("[PASS] %s=%.6g (criterion %s %.6g)\n", n, v, op, thr);
    } else {
        g_fail++;
        printf("[FAIL] %s=%.6g (criterion %s %.6g)\n", n, v, op, thr);
    }
}

static int64_t now_ms(void) { return (int64_t)GetTickCount64(); }

/* ---- delayed delivery min-heap for relay ---- */
typedef struct {
    int64_t due_ms;
    int order; /* arrival index for stable reorder */
    int n;
    uint8_t *data;
} relay_item;

typedef struct {
    relay_item *a;
    int n, cap;
} relay_heap;

static int rh_less(const relay_item *x, const relay_item *y) {
    if (x->due_ms != y->due_ms) return x->due_ms < y->due_ms;
    return x->order < y->order;
}

static void rh_init(relay_heap *h, int cap) {
    h->cap = cap;
    h->n = 0;
    h->a = calloc((size_t)cap, sizeof(relay_item));
}
static void rh_free(relay_heap *h) {
    for (int i = 0; i < h->n; i++) free(h->a[i].data);
    free(h->a);
    h->a = NULL;
    h->n = 0;
}
static int rh_push(relay_heap *h, relay_item it) {
    if (h->n >= h->cap) return -1;
    int i = h->n++;
    h->a[i] = it;
    while (i > 0) {
        int p = (i - 1) / 2;
        if (!rh_less(&h->a[i], &h->a[p])) break;
        relay_item t = h->a[i];
        h->a[i] = h->a[p];
        h->a[p] = t;
        i = p;
    }
    return 0;
}
static int rh_pop(relay_heap *h, relay_item *out) {
    if (h->n <= 0) return -1;
    *out = h->a[0];
    h->n--;
    if (h->n > 0) {
        h->a[0] = h->a[h->n];
        int i = 0;
        for (;;) {
            int l = 2 * i + 1, r = l + 1, m = i;
            if (l < h->n && rh_less(&h->a[l], &h->a[m])) m = l;
            if (r < h->n && rh_less(&h->a[r], &h->a[m])) m = r;
            if (m == i) break;
            relay_item t = h->a[i];
            h->a[i] = h->a[m];
            h->a[m] = t;
            i = m;
        }
    }
    return 0;
}

/* ---- UDP loopback smoke ---- */
static int test_udp_loopback(void) {
    if (net_init() != 0) {
        report("udp_wsa", 0.0, "==", 1.0, 0);
        return -1;
    }
    udp_sock a, b;
    if (udp_open(&a) != 0 || udp_open(&b) != 0) {
        report("udp_open", 0.0, "==", 1.0, 0);
        return -1;
    }
    uint16_t pa = 0, pb = 0;
    int bound = 0;
    for (int base = 41000; base < 41100 && !bound; base++) {
        if (udp_bind(&a, (uint16_t)base, &pa) == 0) {
            for (int b2 = base + 1; b2 < base + 50; b2++) {
                if (udp_bind(&b, (uint16_t)b2, &pb) == 0) {
                    bound = 1;
                    break;
                }
            }
        }
    }
    if (!bound) {
        /* ephemeral fallback */
        if (udp_bind(&a, 0, &pa) != 0 || udp_bind(&b, 0, &pb) != 0) {
            report("udp_bind", 0.0, "==", 1.0, 0);
            return -1;
        }
    }
    uint8_t msg[] = "lab18-udp-ping";
    int sent = udp_sendto(&a, msg, (int)sizeof(msg), pb);
    uint8_t rbuf[64];
    int got = udp_recv(&b, rbuf, (int)sizeof(rbuf), 500);
    int ok = (sent == (int)sizeof(msg)) && (got == sent) && memcmp(rbuf, msg, (size_t)got) == 0;
    printf("  info: udp loopback ports %u->%u sent=%d got=%d\n", pa, pb, sent, got);
    report("udp_loopback", ok ? 1.0 : 0.0, "==", 1.0, ok);
    udp_close(&a);
    udp_close(&b);
    return ok ? 0 : -1;
}

/* ---- channel relay + adaptive JB over real UDP ---- */
typedef struct {
    udp_sock *rx; /* bound relay port, receives from sender */
    uint16_t dest_port;
    double loss_rate;
    int max_delay_ms;
    int reorder_pct;
    volatile int stop;
    int received;
    int dropped;
    int forwarded;
    unsigned rng;
    relay_heap heap;
    pthread_t th;
} relay_ctx;

static unsigned lcg_next(unsigned *s) {
    *s = (*s) * 1103515245u + 12345u;
    return (*s >> 16) & 0x7fff;
}

static void *relay_fn(void *arg) {
    relay_ctx *r = arg;
    udp_sock out;
    if (udp_open(&out) != 0) return NULL;
    int order = 0;
    while (!r->stop) {
        uint8_t buf[2048];
        int n = udp_recv(r->rx, buf, (int)sizeof(buf), 5);
        if (n > 0) {
            r->received++;
            double u = (lcg_next(&r->rng) % 1000) / 1000.0;
            if (u < r->loss_rate) {
                r->dropped++;
            } else {
                int d = (int)(lcg_next(&r->rng) % (r->max_delay_ms + 1));
                /* extra reorder: sometimes push delay higher so order flips */
                if ((int)(lcg_next(&r->rng) % 100) < r->reorder_pct)
                    d += r->max_delay_ms / 2 + 1;
                relay_item it;
                it.due_ms = now_ms() + d;
                it.order = order++;
                it.n = n;
                it.data = malloc((size_t)n);
                if (it.data) {
                    memcpy(it.data, buf, (size_t)n);
                    if (rh_push(&r->heap, it) != 0) free(it.data);
                }
            }
        }
        /* deliver due */
        int64_t t = now_ms();
        relay_item it;
        while (r->heap.n > 0 && r->heap.a[0].due_ms <= t) {
            if (rh_pop(&r->heap, &it) != 0) break;
            udp_sendto(&out, it.data, it.n, r->dest_port);
            r->forwarded++;
            free(it.data);
        }
    }
    /* flush remaining soon-due packets */
    int64_t deadline = now_ms() + 300;
    while (r->heap.n > 0 && now_ms() < deadline) {
        int64_t t = now_ms();
        relay_item it;
        while (r->heap.n > 0 && r->heap.a[0].due_ms <= t) {
            if (rh_pop(&r->heap, &it) != 0) break;
            udp_sendto(&out, it.data, it.n, r->dest_port);
            r->forwarded++;
            free(it.data);
        }
        if (r->heap.n > 0) Sleep(2);
    }
    udp_close(&out);
    return NULL;
}

#define NP 280
#define PAYLOAD_BYTES 32

static int test_udp_adaptive_jb(void) {
    if (net_init() != 0) return -1;
    udp_sock rx, send;
    if (udp_open(&rx) != 0 || udp_open(&send) != 0) return -1;
    uint16_t rx_port = 0, relay_port = 0;
    int bound = 0;
    for (int base = 42000; base < 42200 && !bound; base++) {
        if (udp_bind(&rx, (uint16_t)base, &rx_port) != 0) continue;
        if (udp_bind(&send, (uint16_t)(base + 1), &relay_port) == 0) {
            bound = 1;
            break;
        }
        udp_close(&rx);
        udp_close(&send);
        if (udp_open(&rx) != 0 || udp_open(&send) != 0) return -1;
    }
    if (!bound) {
        if (udp_bind(&rx, 0, &rx_port) != 0) return -1;
        if (udp_bind(&send, 0, &relay_port) != 0) return -1;
    }

    relay_ctx rc;
    memset(&rc, 0, sizeof(rc));
    rc.rx = &send;
    rc.dest_port = rx_port;
    rc.loss_rate = 0.10;
    rc.max_delay_ms = 100;
    rc.reorder_pct = 15;
    rc.stop = 0;
    rc.rng = 42u;
    rh_init(&rc.heap, NP + 8);
    pthread_create(&rc.th, NULL, relay_fn, &rc);

    /* send packets to relay_port via a third socket */
    udp_sock tx;
    if (udp_open(&tx) != 0) {
        rc.stop = 1;
        pthread_join(rc.th, NULL);
        return -1;
    }

    ajb jb;
    ajb_init(&jb, NP + 16);

    int64_t arrival[NP];
    int64_t sent_ms[NP];
    for (int i = 0; i < NP; i++) { arrival[i] = -1; sent_ms[i] = -1; }
    int sent = 0;
    uint8_t pay[PAYLOAD_BYTES];
    memset(pay, 0xA5, sizeof(pay));

    /* send burst; receive concurrently on main */
    int received_pkts = 0;
    for (int i = 0; i < NP; i++) {
        rtp_pkt p;
        p.seq = (uint16_t)i;
        p.ts = (uint32_t)i * 160;
        p.pt = 96;
        p.marker = 0;
        p.ssrc = 0xc0ffee;
        p.send_ms = now_ms();
        p.payload = pay;
        p.len = PAYLOAD_BYTES;
        sent_ms[i] = p.send_ms;
        uint8_t wbuf[256];
        int wn = rtp_pack(&p, wbuf, (int)sizeof(wbuf));
        if (wn > 0 && udp_sendto(&tx, wbuf, wn, relay_port) > 0) sent++;

        /* drain some arrivals */
        for (int k = 0; k < 2; k++) {
            uint8_t rbuf[512];
            int n = udp_recv(&rx, rbuf, (int)sizeof(rbuf), 0);
            if (n <= 0) break;
            rtp_pkt q;
            if (rtp_unpack(rbuf, n, &q) == 0 && q.seq < NP) {
                int64_t t = now_ms();
                arrival[q.seq] = t;
                ajb_push(&jb, &q, t);
                received_pkts++;
            }
        }
    }
    /* wait for relay flush */
    int64_t wait_end = now_ms() + 400;
    while (now_ms() < wait_end) {
        uint8_t rbuf[512];
        int n = udp_recv(&rx, rbuf, (int)sizeof(rbuf), 5);
        if (n > 0) {
            rtp_pkt q;
            if (rtp_unpack(rbuf, n, &q) == 0 && q.seq < NP) {
                int64_t t = now_ms();
                arrival[q.seq] = t;
                ajb_push(&jb, &q, t);
                received_pkts++;
            }
        }
    }
    rc.stop = 1;
    pthread_join(rc.th, NULL);

    /* Adaptive playout simulation:
       start_play = first arrival + target_ms
       frame interval 20ms
       underrun if expected seq not in buffer at its play deadline */
    int64_t first = -1;
    for (int i = 0; i < NP; i++)
        if (arrival[i] >= 0 && (first < 0 || arrival[i] < first)) first = arrival[i];

    int lost = 0, late = 0, played = 0;
    int target = jb.target_ms;
    for (int i = 0; i < NP; i++) {
        if (arrival[i] < 0) {
            lost++;
            continue;
        }
        int64_t deadline = first + target + (int64_t)i * 20;
        if (arrival[i] > deadline) late++;
        else played++;
    }
    int arrived = NP - lost;
    double under_rate = arrived > 0 ? (100.0 * late / arrived) : 100.0;
    /* e2e delay: mean(recv - send) of played packets ≈ delay_ema */
    double e2e = jb.delay_ema;

    /* GCC consuming the MEASURED channel: one-way-delay gradient from real
     * UDP arrivals + measured relay loss. With ~10% loss the loss branch
     * must hold the rate down instead of ramping to the 1.2 max. */
    double loss_meas = rc.received > 0 ? 100.0 * rc.dropped / rc.received : 0.0;
    gcc_t gm;
    gcc_init(&gm);
    double base_owd = -1;
    for (int i = 0; i < NP; i++) {
        if (arrival[i] < 0 || sent_ms[i] < 0) continue;
        double owd = (double)(arrival[i] - sent_ms[i]);
        if (base_owd < 0) base_owd = owd;
        gcc_update(&gm, owd - base_owd, loss_meas);
    }
    printf("  info: gcc on measured channel: loss=%.1f%% rate=%.3f decreases=%d loss_events=%d\n",
           loss_meas, gm.rate, gm.decrease_count, gm.loss_events);

    printf("  info: sent=%d relay_recv=%d dropped=%d forwarded=%d arrived=%d\n", sent,
           rc.received, rc.dropped, rc.forwarded, arrived);
    printf("  info: adaptive target_ms=%d jitter=%.2f e2e_delay_ms=%.2f late=%d lost=%d played=%d\n",
           target, jb.jitter, e2e, late, lost, played);

    int ok_under = under_rate < 1.0;
    int ok_e2e = e2e > 0.0 && e2e < 200.0;
    int ok_udp = received_pkts > 50 && rc.forwarded > 50;
    int ok_gcc_meas = gm.rate <= 0.9 && gm.decrease_count >= 1;
    report("udp_datagrams", (double)received_pkts, ">", 50.0, ok_udp);
    report("jb_underrun_pct", under_rate, "<", 1.0, ok_under);
    report("jb_e2e_delay_ms", e2e, "<", 200.0, ok_e2e);
    report("ajb_target_ms", (double)target, ">=", 20.0, target >= 20);
    report("gcc_on_measured_channel", gm.rate, "<=", 0.9, ok_gcc_meas);

    ajb_free(&jb);
    rh_free(&rc.heap);
    udp_close(&tx);
    udp_close(&rx);
    udp_close(&send);
    return (ok_under && ok_e2e && ok_udp && ok_gcc_meas) ? 0 : -1;
}

/* ---- GCC scripted congestion: gradient onset BEFORE loss, then a lossy
 * phase where the loss branch (multiplicative decrease) must act ---- */
static int test_gcc_congestion(void) {
    gcc_t g;
    gcc_init(&g);
    double rate_before = 0, rate_at_onset = 0, rate_after_grad = 0, rate_after_loss = 0;
    /* phase 1 free (0-39), short rising gradient (40-59) — the decrease must
       begin BEFORE any loss; lossy congestion (60-74) — the loss branch must
       push the rate below the gradient level; long recovery (75-199) */
    const int n_samp = 200;
    for (int i = 0; i < n_samp; i++) {
        double grad;
        double loss;
        if (i < 40) {
            grad = 0.2;
            loss = 1.0;
        } else if (i < 56) {
            grad = 2.0 + 0.5 * (i - 40); /* congestion: gradient rises before loss */
            loss = 1.0;
        } else if (i < 75) {
            grad = 8.0;
            loss = 25.0; /* queue overflow: real packet loss */
        } else {
            grad = 0.4; /* recovered path */
            loss = 1.0;
        }
        if (i == 39) rate_before = g.rate;
        double r = gcc_update(&g, grad, loss);
        if (i == 54) rate_at_onset = r; /* gradient phase, before any loss */
        if (i == 55) rate_after_grad = r;
        if (i == 74) rate_after_loss = r;
    }
    int ok_drop = (rate_at_onset < rate_before) && (rate_after_grad < rate_before * 0.9);
    int ok_loss = (rate_after_loss < rate_after_grad); /* loss branch acts */
    int ok_state = g.decrease_count >= 1 && g.loss_events >= 1;
    double util = g.rate / 1.2;
    printf("  info: gcc before=%.3f onset=%.3f after_grad=%.3f after_loss=%.3f end=%.3f\n",
           rate_before, rate_at_onset, rate_after_grad, rate_after_loss, g.rate);
    printf("  info: gcc decreases=%d loss_events=%d increases=%d util_est=%.1f%%\n",
           g.decrease_count, g.loss_events, g.increase_count, util * 100.0);
    report("gcc_rate_drops_on_congestion", rate_at_onset, "<", rate_before, ok_drop);
    report("gcc_loss_branch_rate", rate_after_loss, "<", rate_after_grad, ok_loss);
    report("gcc_decrease_events", (double)g.decrease_count, ">=", 1.0, ok_state);
    /* Roadmap: after congestion, steady utilization ≥80% of max (rate ≥0.96 of 1.2). */
    int ok_util = util >= 0.80;
    report("gcc_util_after_recovery", util, ">=", 0.80, ok_util);
    report("gcc_recover_rate", g.rate, ">=", 0.90, g.rate >= 0.90);
    return (ok_drop && ok_loss && ok_state && ok_util && g.rate >= 0.90) ? 0 : -1;
}

/* ---- PLC 3-tier ---- */
static int test_plc(void) {
    const int n = 400;
    double sig[400];
    int lost[400];
    for (int i = 0; i < n; i++) {
        sig[i] = sin(0.2 * i) + 0.3 * sin(0.05 * i);
        lost[i] = (i % 10 == 3);
    }
    double e0 = 0, e1 = 0, e2 = 0;
    double prev = 0;
    for (int i = 0; i < n; i++) {
        double r0 = lost[i] ? 0.0 : sig[i];
        double r1 = lost[i] ? prev : sig[i];
        double r2 = lost[i] ? (i > 1 ? 2 * sig[i - 1] - sig[i - 2] : 0.0) : sig[i];
        if (!lost[i]) prev = sig[i];
        e0 += (r0 - sig[i]) * (r0 - sig[i]);
        e1 += (r1 - sig[i]) * (r1 - sig[i]);
        e2 += (r2 - sig[i]) * (r2 - sig[i]);
    }
    printf("  info: PLC MSE silence=%.1f hold=%.1f extrap=%.1f\n", e0, e1, e2);
    int ok = e2 < e1 && e1 < e0;
    report("plc_ranking_extrap_lt_hold_lt_silence", e2, "<", e1, ok);
    return ok ? 0 : -1;
}

/* ---- A/V sync: audio master clock + video early/late decisions ----.
 * Both tracks share one delay channel. The audio master plays content M
 * at wall M + L (L = mean channel delay + playout buffer). A video frame
 * with content M_k arriving at wall M_k + d_k + D is on time while
 * M_k + d_k + D < M_k + L, i.e. late by (d_k + D − L). The sync module
 * must display every frame whose lateness is within tol (1 video frame)
 * and drop stale ones. Two runs: D=0 (no forced drops) and D=100 ms
 * (video path lagging → module must act). */
static int av_sync_run(int video_delay_ms, int *out_dropped, int *out_nv) {
    const int na = 60, nv = 18;
    const double L = 90.0; /* audio playout latency: mean delay 50 + JB target 40 */
    unsigned rng = 7u;
    int64_t a_arr[60];
    for (int i = 0; i < na; i++) {
        int delay = (int)(lcg_next(&rng) % 100);
        if ((int)(lcg_next(&rng) % 100) < 10) { a_arr[i] = -1; continue; }
        a_arr[i] = (int64_t)i * 20 + delay;
    }
    unsigned rv = 9u;
    int64_t v_arr[18];
    for (int i = 0; i < nv; i++) {
        int delay = (int)(lcg_next(&rv) % 100);
        if ((int)(lcg_next(&rv) % 100) < 10) { v_arr[i] = -1; continue; }
        v_arr[i] = (int64_t)i * 33 + delay + video_delay_ms;
    }
    avsync s;
    avsync_init(&s, 0.0, 33.0); /* display at content time; L lives in the master clock */
    /* merge both arrival timelines in wall-clock order */
    int ai = 0, vi = 0, dropped = 0, displayed = 0;
    double max_display_late = 0;
    while (ai < na || vi < nv) {
        int64_t ta = (ai < na && a_arr[ai] >= 0) ? a_arr[ai] : INT64_MAX;
        int64_t tv = (vi < nv && v_arr[vi] >= 0) ? v_arr[vi] : INT64_MAX;
        /* advance audio on ties (and on double-INT64_MAX, else vi would
         * never advance past a lost trailing frame) */
        if (ai < na && ta <= tv) {
            ai++; /* audio frame: advances the master clock (media ms) */
            continue;
        }
        if (tv < INT64_MAX) { /* lost video frames never reach the sync module */
            double audio_master = (double)tv - L; /* content time audio is playing */
            double video_ms = (double)vi * 33.333;
            double due_in;
            int dec = avsync_frame(&s, video_ms, audio_master, &due_in);
            if (dec == 0) dropped++;
            if (dec == 1) {
                displayed++;
                /* displayed frames must be within one video frame of sync */
                double late = -due_in;
                if (late > max_display_late) max_display_late = late;
            }
        }
        vi++;
    }
    *out_dropped = dropped;
    *out_nv = nv;
    printf("  info: av_sync D=%dms -> displayed=%d dropped=%d held=%d skew=%.1fms max_display_late=%.1fms\n",
           video_delay_ms, s.displayed, s.dropped, s.held, s.skew_ema_ms, max_display_late);
    (void)displayed;
    return (max_display_late <= 33.0) ? 0 : -1;
}

static int test_av_sync(void) {
    int dropped0 = 0, droppedD = 0, nv = 0;
    int ok0 = av_sync_run(0, &dropped0, &nv) == 0;
    int okD = av_sync_run(100, &droppedD, &nv) == 0;
    printf("  info: forced drops: D=0 -> %d, D=100ms -> %d (of %d frames)\n",
           dropped0, droppedD, nv);
    report("av_sync_baseline_no_drops", (double)dropped0, "==", 0.0, ok0 && dropped0 == 0);
    report("av_sync_display_within_frame", 1.0, "==", 1.0, ok0 && okD);
    /* with video lagging 100 ms, the sync module must act on stale frames */
    report("av_sync_drops_on_video_lag", (double)droppedD, ">=", 1.0,
           okD && droppedD >= 1);
    return (ok0 && okD && dropped0 == 0 && droppedD >= 1) ? 0 : -1;
}

static int test_rtp_roundtrip(void) {
    uint8_t payload[32];
    for (int i = 0; i < 32; i++) payload[i] = (uint8_t)(i * 3);
    rtp_pkt p = {.seq = 7, .ts = 12345, .pt = 111, .marker = 1,
                 .ssrc = 0x1234abcd, .payload = payload, .len = 32,
                 .send_ms = 999999};
    uint8_t buf[64];
    int n = rtp_pack(&p, buf, 64);
    rtp_pkt q;
    if (rtp_unpack(buf, n, &q) != 0) return -1;
    int ok = (q.seq == 7 && q.ts == 12345 && q.pt == 111 && q.len == 32 &&
              q.send_ms == 999999 && q.marker == 1 && q.ssrc == 0x1234abcd &&
              memcmp(q.payload, payload, 32) == 0);
    /* header layout: V=2, marker at bit7 of byte1, SSRC at bytes 8-11 */
    int hdr_ok = ((buf[0] >> 6) == 2) && (((buf[1] >> 7) & 1) == 1) &&
                 buf[8] == 0x12 && buf[11] == 0xcd;
    report("rtp_roundtrip", ok ? 1.0 : 0.0, "==", 1.0, ok);
    report("rtp_header_fields", hdr_ok ? 1.0 : 0.0, "==", 1.0, hdr_ok);
    return (ok && hdr_ok) ? 0 : -1;
}

static int run_selftest(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== lab18 selftest ===\n");
    if (test_rtp_roundtrip() != 0) g_fail++;
    if (test_udp_loopback() != 0) g_fail++;
    if (test_udp_adaptive_jb() != 0) g_fail++;
    if (test_gcc_congestion() != 0) g_fail++;
    if (test_plc() != 0) g_fail++;
    if (test_av_sync() != 0) g_fail++;
    printf("summary: %d passed, %d failed\n", g_pass, g_fail);
    net_cleanup();
    if (g_fail == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    }
    printf("FAILED %d/%d\n", g_fail, g_pass + g_fail);
    return 1;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc > 1 && (!strcmp(argv[1], "--selftest") || !strcmp(argv[1], "--generate")))
        return run_selftest();
    printf("usage: %s --selftest\n", argv[0]);
    return 2;
}
