/* lab19 E2E: capture → VAD → AEC → NS → AGC → ADPCM → channel → JB/PLC → decode → WAV/PPM
 * dual-thread bounded vs unbounded + probe localizer + audio-master A/V sync */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "adpcm.h"
#include "agc.h"
#include "aec_lite.h"
#include "avsync_lite.h"
#include "channel_lite.h"
#include "ns_lite.h"
#include "pipeline.h"
#include "ppm_io.h"
#include "rtp_lite.h"
#include "vad_lite.h"
#include "vid_codec.h"
#include "wav_io.h"

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

#define FRAME_LEN 160      /* 10 ms @ 16 kHz */
#define MEDIA_MS 10        /* media time per frame */
#define NFRAMES 120
#define NEAR_LO 40         /* near-end speech frames [40, 80) */
#define NEAR_HI 80
#define JB_TARGET_MS 40    /* jitter buffer playout target */
#define CH_DELAY_MS 25     /* channel max one-way delay */

/* ---------- full chain (single-thread, media-time-scheduled for probes) ---- */
typedef struct {
    probe_stat stages[8];
    int nstages;
    double *out_pcm;
    int out_n;
    int packets_sent;
    int packets_delivered;
    int plc_frames;
    double erle_db;      /* far-only tail: mic vs AEC output */
    double e2e_mean_ms;
    double e2e_max_ms;
    int vad_frames;
} chain_result;

static int run_chain(int inject_bug, chain_result *cr) {
    memset(cr, 0, sizeof(*cr));
    probe_reset(&cr->stages[0], "capture");
    probe_reset(&cr->stages[1], "vad");
    probe_reset(&cr->stages[2], "aec");
    probe_reset(&cr->stages[3], "ns");
    probe_reset(&cr->stages[4], "agc");
    probe_reset(&cr->stages[5], "adpcm_encode");
    probe_reset(&cr->stages[6], "jb_decode");
    probe_reset(&cr->stages[7], "output");
    cr->nstages = 8;

    vad_lite vad;
    vad_lite_init(&vad, 8.0, 6);
    aec_nlms aec;
    aec_nlms_init(&aec, 64, 0.3);
    ns_lite ns;
    ns_lite_init(&ns, 0.15);
    agc a;
    agc_init(&a, -18.0, 16000.0, 0.01, 0.10);
    if (inject_bug) a.injected_scale = 12.0; /* wrong gain */

    channel ch;
    ch_init(&ch, NFRAMES + 16, 0.10, CH_DELAY_MS, 15, 7u);
    jb_lite jb;
    jb_init(&jb, NFRAMES + 16);

    cr->out_pcm = calloc((size_t)NFRAMES * FRAME_LEN, sizeof(double));
    if (!cr->out_pcm) return -1;

    /* synthetic acoustic scene: far speech → echo path → mic; near speech
     * added mid-stream; noise floor throughout */
    unsigned rng = 123u;
    double h[64];
    for (int k = 0; k < 64; k++)
        h[k] = 0.05 * exp(-(double)k / 16.0) *
               (((rng = rng * 1103515245u + 12345u) >> 16) & 0xffff) / 32768.0;
    double mic[NFRAMES * FRAME_LEN];
    double far_sig[NFRAMES * FRAME_LEN];
    double y = 0;
    for (int i = 0; i < NFRAMES * FRAME_LEN; i++) {
        rng = rng * 1103515245u + 12345u;
        double e = ((rng >> 16) & 0x7fff) / 16384.0 - 1.0;
        y = 0.95 * y + e;
        double env = 0.5 + 0.5 * sin(2.0 * 3.14159265358979 * 2.0 * i / 16000.0);
        far_sig[i] = 0.15 * env * y;
        double echo = 0;
        for (int k = 0; k < 64 && k <= i; k++) echo += h[k] * far_sig[i - k];
        double near = 0.0;
        if (i >= NEAR_LO * FRAME_LEN && i < NEAR_HI * FRAME_LEN)
            near = 0.3 * sin(2.0 * 3.14159265358979 * 250.0 * i / 16000.0);
        rng = rng * 1103515245u + 12345u;
        double noise = 0.008 * (((rng >> 16) & 0x7fff) / 16384.0 - 1.0);
        mic[i] = echo + near + noise;
    }

    double erle_mic = 0, erle_aec = 0; /* far-only tail [NEAR_HI, NFRAMES) */
    double e2e_sum = 0;
    int e2e_n = 0;
    int next_play = 0;
    double prev_pcm[FRAME_LEN]; /* PLC hold buffer */
    memset(prev_pcm, 0, sizeof(prev_pcm));
    int flush_ticks = (CH_DELAY_MS * 2 + JB_TARGET_MS) / MEDIA_MS + 8;

    for (int f = 0; f < NFRAMES + flush_ticks; f++) {
        int t = f * MEDIA_MS;
        const double *frame;
        double framebuf[FRAME_LEN];
        if (f < NFRAMES) {
            frame = mic + (size_t)f * FRAME_LEN;
        } else {
            memset(framebuf, 0, sizeof(framebuf));
            frame = framebuf; /* sender idle; keep draining channel/jb */
        }

        if (f < NFRAMES) {
            probe_accum(&cr->stages[0], frame, FRAME_LEN);

            /* VAD on the mic: gates the AGC; also counts speech presence */
            int is_speech = vad_lite_frame(&vad, frame, FRAME_LEN);
            if (is_speech && f > 0) cr->vad_frames++;
            probe_accum(&cr->stages[1], frame, FRAME_LEN);

            /* AEC: adaptive echo cancel against the far reference. Freeze the
             * adaptation when the mic is near-end dominant (signal-driven). */
            double fe = 0, me = 0;
            for (int i = 0; i < FRAME_LEN; i++) {
                fe += far_sig[(size_t)f * FRAME_LEN + i] * far_sig[(size_t)f * FRAME_LEN + i];
                me += frame[i] * frame[i];
            }
            int near_dominant = (me > 3.0 * fe + 1e-9);
            double aec_out[FRAME_LEN];
            for (int i = 0; i < FRAME_LEN; i++)
                aec_out[i] = aec_nlms_step(&aec, far_sig[(size_t)f * FRAME_LEN + i],
                                           frame[i], near_dominant);
            probe_accum(&cr->stages[2], aec_out, FRAME_LEN);
            if (f >= NEAR_HI) {
                for (int i = 0; i < FRAME_LEN; i++) {
                    erle_mic += frame[i] * frame[i];
                    erle_aec += aec_out[i] * aec_out[i];
                }
            }

            /* NS + AGC (gain update gated by VAD to avoid pumping silence) */
            double ns_out[FRAME_LEN], agc_out[FRAME_LEN];
            for (int i = 0; i < FRAME_LEN; i++) ns_out[i] = ns_lite_process(&ns, aec_out[i]);
            probe_accum(&cr->stages[3], ns_out, FRAME_LEN);
            for (int i = 0; i < FRAME_LEN; i++) {
                double v = ns_out[i];
                double g0 = a.gain;
                double gv = agc_process(&a, v);
                if (!is_speech) a.gain = g0; /* hold gain during silence */
                agc_out[i] = gv;
            }
            probe_accum(&cr->stages[4], agc_out, FRAME_LEN);

            /* ADPCM encode + packetize + send into the channel */
            int16_t pcm[FRAME_LEN];
            uint8_t code[FRAME_LEN / 2];
            for (int i = 0; i < FRAME_LEN; i++) {
                double v = agc_out[i];
                if (v > 0.98) v = 0.98; /* encoder input bound (≈ -0.2 dBFS) */
                if (v < -0.98) v = -0.98;
                pcm[i] = (int16_t)lrint(v * 32767.0);
            }
            adpcm_encode(pcm, FRAME_LEN, code);
            double enc_d[FRAME_LEN];
            for (int i = 0; i < FRAME_LEN; i++) enc_d[i] = pcm[i] / 32767.0;
            probe_accum(&cr->stages[5], enc_d, FRAME_LEN);

            pkt p;
            p.seq = (uint16_t)(f & 0xffff);
            p.ts = (uint32_t)(f * FRAME_LEN);
            p.pt = 8;
            p.payload = code;
            p.len = FRAME_LEN / 2;
            uint8_t wbuf[256];
            int wn = pkt_pack(&p, wbuf, (int)sizeof(wbuf));
            if (wn > 0) {
                ch_send(&ch, t, f, wbuf, wn);
                cr->packets_sent++;
            }
        }

        /* receiver: drain arrivals, play what is due, PLC on gaps */
        uint8_t rbuf[256];
        int seq;
        int rlen;
        while (ch_recv(&ch, t, &seq, rbuf, (int)sizeof(rbuf), &rlen) == 0) {
            pkt q;
            if (pkt_unpack(rbuf, rlen, &q) == 0) {
                uint8_t *copy = malloc((size_t)(q.len > 0 ? q.len : 1));
                if (!copy) return -1;
                memcpy(copy, q.payload, (size_t)q.len);
                q.payload = copy;
                jb_insert(&jb, q);
                cr->packets_delivered++;
            }
        }
        while (next_play * MEDIA_MS + JB_TARGET_MS <= t && next_play < NFRAMES) {
            pkt got;
            double dec_d[FRAME_LEN];
            if (jb_take(&jb, (uint16_t)next_play, &got) == 0) {
                int16_t outp[FRAME_LEN];
                adpcm_decode(got.payload, FRAME_LEN, outp);
                for (int i = 0; i < FRAME_LEN; i++) {
                    double v = outp[i] / 32767.0;
                    if (v > 0.98) v = 0.98; /* ADPCM predictor overshoot guard */
                    if (v < -0.98) v = -0.98;
                    dec_d[i] = v;
                }
                memcpy(prev_pcm, dec_d, sizeof(dec_d));
                free(got.payload);
            } else {
                for (int i = 0; i < FRAME_LEN; i++)
                    dec_d[i] = prev_pcm[i]; /* PLC: hold last frame */
                cr->plc_frames++;
            }
            double e2e = (double)t - (double)next_play * MEDIA_MS;
            e2e_sum += e2e;
            e2e_n++;
            if (e2e > cr->e2e_max_ms) cr->e2e_max_ms = e2e;
            probe_accum(&cr->stages[6], dec_d, FRAME_LEN);
            probe_accum(&cr->stages[7], dec_d, FRAME_LEN);
            memcpy(cr->out_pcm + (size_t)next_play * FRAME_LEN, dec_d, sizeof(dec_d));
            next_play++;
        }
    }
    cr->e2e_mean_ms = e2e_n ? e2e_sum / e2e_n : 0.0;
    cr->erle_db = (erle_aec > 1e-12) ? 10.0 * log10(erle_mic / erle_aec) : 0.0;
    cr->out_n = NFRAMES * FRAME_LEN;
    jb_free(&jb);
    ch_free(&ch);
    aec_nlms_free(&aec);
    return 0;
}

static int test_full_chain(void) {
    chain_result cr;
    if (run_chain(0, &cr) != 0) return -1;
    printf("  info: chain packets %d/%d plc=%d vad_frames=%d\n",
           cr.packets_delivered, cr.packets_sent, cr.plc_frames, cr.vad_frames);
    printf("  info: chain ERLE (far-only tail)=%.2f dB  e2e media delay mean=%.1f ms max=%.1f ms\n",
           cr.erle_db, cr.e2e_mean_ms, cr.e2e_max_ms);
    /* write artifacts */
    wav_data w;
    if (wav_from_doubles(cr.out_pcm, (size_t)cr.out_n, 16000, &w) == 0) {
        wav_write("out/chain_out.wav", &w);
        wav_free(&w);
    }
    /* energy-over-time PPM strip */
    int wpx = 64, hpx = 32;
    uint8_t *rgb = calloc((size_t)wpx * hpx * 3, 1);
    if (rgb) {
        for (int x = 0; x < wpx; x++) {
            int i0 = x * cr.out_n / wpx;
            int i1 = (x + 1) * cr.out_n / wpx;
            if (i1 <= i0) i1 = i0 + 1;
            double e = 0;
            for (int i = i0; i < i1 && i < cr.out_n; i++) e += cr.out_pcm[i] * cr.out_pcm[i];
            e = sqrt(e / (i1 - i0));
            int y0 = (int)(e * 4.0 * hpx);
            if (y0 > hpx) y0 = hpx;
            for (int y = 0; y < hpx; y++) {
                int idx = ((hpx - 1 - y) * wpx + x) * 3;
                uint8_t v = (y < y0) ? 200 : 20;
                rgb[idx] = v;
                rgb[idx + 1] = (uint8_t)(v / 2 + 40);
                rgb[idx + 2] = 40;
            }
        }
        ppm_write("out/chain_energy.ppm", rgb, wpx, hpx);
        free(rgb);
    }
    double loss_rate = cr.packets_sent ? 1.0 - (double)cr.packets_delivered / cr.packets_sent : 1.0;
    int ok_pkts = loss_rate <= 0.15; /* ~10% channel loss tolerated */
    int ok_erle = cr.erle_db >= 15.0;
    int ok_vad = cr.vad_frames > 0;
    int ok_plc = cr.plc_frames > 0; /* channel loss was hidden by PLC */
    int ok_e2e = cr.e2e_mean_ms > 0 && cr.e2e_mean_ms < 200.0 && cr.e2e_max_ms < 100.0;
    double peak = 0, energy = 0;
    for (int i = 0; i < cr.out_n; i++) {
        double a = fabs(cr.out_pcm[i]);
        if (a > peak) peak = a;
        energy += cr.out_pcm[i] * cr.out_pcm[i];
    }
    int ok_out = (peak > 0.01 && peak <= 1.0 && energy > 1.0);
    report("chain_loss_rate_pct", 100.0 * loss_rate, "<=", 15.0, ok_pkts);
    report("chain_erle_db", cr.erle_db, ">=", 15.0, ok_erle);
    report("chain_vad_active_frames", (double)cr.vad_frames, ">", 0.0, ok_vad);
    report("chain_plc_frames", (double)cr.plc_frames, ">", 0.0, ok_plc);
    report("chain_e2e_delay_mean_ms", cr.e2e_mean_ms, "<", 200.0, ok_e2e);
    report("chain_e2e_delay_max_ms", cr.e2e_max_ms, "<", 100.0, cr.e2e_max_ms < 100.0);
    report("chain_output_energy", energy, ">", 1.0, ok_out);
    free(cr.out_pcm);
    return (ok_pkts && ok_erle && ok_vad && ok_plc && ok_e2e && ok_out) ? 0 : -1;
}

/* roadmap: integrated AGC must keep its convergence spec (no regression) */
static int test_agc_integration(void) {
    const double fs = 16000;
    const int n = 48000; /* 3 s, +20 dB step at t=1s */
    agc a;
    agc_init(&a, -18.0, fs, 0.01, 0.10);
    double *y = calloc((size_t)n, sizeof(double));
    if (!y) return -1;
    for (int i = 0; i < n; i++) {
        double t = i / fs;
        double amp = (t < 1.0) ? 0.05 : 0.5;
        y[i] = agc_process(&a, 0.0 * 0 + amp * sin(2.0 * 3.14159265358979 * 400.0 * t));
    }
    int lo = 16000 + 8000, hi = 16000 + 16000; /* [jump+0.5s, jump+1.0s] */
    double pk = 0;
    for (int i = lo; i < hi; i++) {
        double a2 = fabs(y[i]);
        if (a2 > pk) pk = a2;
    }
    double pk_db = 20.0 * log10(pk + 1e-12);
    printf("  info: integrated AGC post-jump peak=%.2f dBFS target=%.1f\n", pk_db, a.target_db);
    int ok = fabs(pk_db - a.target_db) <= 2.0;
    report("integrated_agc_settle_db", fabs(pk_db - a.target_db), "<=", 2.0, ok);
    free(y);
    return ok ? 0 : -1;
}

static int test_probe_localizer(void) {
    chain_result ok, bad;
    if (run_chain(0, &ok) != 0) return -1;
    if (run_chain(1, &bad) != 0) {
        free(ok.out_pcm);
        return -1;
    }
    const char *loc_clean = probe_localize(ok.stages, ok.nstages, 8.0, 5);
    const char *loc_bug = probe_localize(bad.stages, bad.nstages, 8.0, 5);
    printf("  info: clean localizer=%s bug localizer=%s\n", loc_clean ? loc_clean : "(none)",
           loc_bug ? loc_bug : "(none)");
    for (int i = 0; i < bad.nstages; i++) {
        printf("  info: stage[%d] %s energy=%.4f peak=%.3f clipped=%d\n", i,
               bad.stages[i].name, bad.stages[i].energy, bad.stages[i].peak,
               bad.stages[i].clipped);
    }
    int ok_clean = (loc_clean == NULL);
    int ok_bug = (loc_bug != NULL && strcmp(loc_bug, "agc") == 0);
    report("probe_clean_no_false_positive", ok_clean ? 1.0 : 0.0, "==", 1.0, ok_clean);
    report("probe_localizes_agc", ok_bug ? 1.0 : 0.0, "==", 1.0, ok_bug);
    free(ok.out_pcm);
    free(bad.out_pcm);
    return (ok_clean && ok_bug) ? 0 : -1;
}

/* ---------- dual-thread bounded vs unbounded ---------- */
typedef struct {
    queue *q;
    int nframes;
    int produce_us;
} producer_arg;

typedef struct {
    queue *q;
    int processed;
    int consume_us;
    int64_t max_queue_delay_ms;
} consumer_arg;

static int64_t wall_ms(void) {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void *producer_fn(void *p) {
    producer_arg *a = p;
    for (int i = 0; i < a->nframes; i++) {
        frame f;
        f.len = FRAME_LEN;
        f.seq = i;
        f.data = malloc((size_t)f.len * sizeof(double));
        if (!f.data) continue;
        for (int k = 0; k < f.len; k++)
            f.data[k] = 0.05 * sin(0.2 * (i * f.len + k));
        f.t_ms = wall_ms();
        q_push(a->q, f);
        if (a->produce_us > 0) usleep((useconds_t)a->produce_us);
    }
    return NULL;
}

static void *consumer_fn(void *p) {
    consumer_arg *a = p;
    for (;;) {
        frame f;
        if (q_pop(a->q, &f) != 0) break;
        if (!f.data) break;
        double e = 0;
        for (int i = 0; i < f.len; i++) e += f.data[i] * f.data[i];
        (void)e;
        int64_t delay = wall_ms() - f.t_ms;
        if (delay > a->max_queue_delay_ms) a->max_queue_delay_ms = delay;
        free(f.data);
        a->processed++;
        if (a->consume_us > 0) usleep((useconds_t)a->consume_us);
    }
    return NULL;
}

static int run_pair(int bounded, int cap, int nframes, int consume_us, int64_t *max_delay,
                    int *dropped, int *processed) {
    queue q;
    q_init(&q, cap, bounded);
    producer_arg pa = {&q, nframes, 0};
    consumer_arg ca = {&q, 0, consume_us, 0};
    pthread_t tp, tc;
    pthread_create(&tp, NULL, producer_fn, &pa);
    pthread_create(&tc, NULL, consumer_fn, &ca);
    pthread_join(tp, NULL);
    q_finish(&q);
    pthread_join(tc, NULL);
    *max_delay = ca.max_queue_delay_ms;
    *dropped = q.dropped;
    *processed = ca.processed;
    q_free(&q);
    return 0;
}

static int test_bounded_vs_unbounded(void) {
    int64_t d_b = 0, d_u = 0;
    int drop_b = 0, drop_u = 0, pr_b = 0, pr_u = 0;
    /* fast produce, slow consume → backlog (5 ms/frame consume vs instant produce) */
    run_pair(1, 4, 80, 5000, &d_b, &drop_b, &pr_b);
    run_pair(0, 4, 80, 5000, &d_u, &drop_u, &pr_u);
    printf("  info: bounded delay=%lld drop=%d processed=%d\n", (long long)d_b, drop_b, pr_b);
    printf("  info: unbounded delay=%lld drop=%d processed=%d\n", (long long)d_u, drop_u, pr_u);
    /* bounded: drops frames, wall-clock wait capped by queue depth × consume */
    int ok_b = (drop_b > 0) && (d_b < 100);
    int ok_u = (d_u >= d_b) && (drop_u == 0) && (d_u > d_b || d_b == 0);
    report("bounded_drops", (double)drop_b, ">", 0.0, drop_b > 0);
    report("bounded_max_delay_ms", (double)d_b, "<", 100.0, ok_b);
    report("unbounded_delay_ge_bounded", (double)d_u, ">=", (double)d_b, ok_u);
    return (ok_b && ok_u) ? 0 : -1;
}

/* ---------- video line: synth → encode → packets → decode → PPM sequence ---------- */
static int test_video_line(void) {
    const int W = 96, H = 72, N = 16, GOP = 16, QP = 8;
    const int npix = W * H;
    uint8_t *frames = malloc((size_t)N * npix);
    uint8_t *enc_rec = malloc((size_t)N * npix);
    uint8_t *dec_rec = malloc((size_t)N * npix);
    int cap = N * npix * 4 + 64;
    uint8_t *pkts = calloc((size_t)cap, 1);
    int *lens = malloc((size_t)N * sizeof(int));
    if (!frames || !enc_rec || !dec_rec || !pkts || !lens) {
        free(frames); free(enc_rec); free(dec_rec); free(pkts); free(lens);
        return -1;
    }
    for (int f = 0; f < N; f++) vid_seq_make(frames + (size_t)f * npix, W, H, f);

    int npkts = vid_encode_gop(frames, enc_rec, W, H, N, GOP, QP, pkts, cap, lens, N);
    printf("  info: video encode %dx%d x%d packets=%d\n", W, H, N, npkts);
    if (npkts != N) {
        free(frames); free(enc_rec); free(dec_rec); free(pkts); free(lens);
        report("video_packets", (double)npkts, "==", (double)N, 0);
        return -1;
    }
    report("video_packets", (double)npkts, "==", (double)N, 1);

    if (vid_decode_gop(pkts, lens, npkts, dec_rec, W, H, N, GOP) != 0) {
        free(frames); free(enc_rec); free(dec_rec); free(pkts); free(lens);
        report("video_decode_ok", 0.0, "==", 1.0, 0);
        return -1;
    }
    report("video_decode_ok", 1.0, "==", 1.0, 1);

    double psnr = vid_psnr(frames + (size_t)(N - 1) * npix,
                           dec_rec + (size_t)(N - 1) * npix, npix);
    double avg = 0;
    for (int f = 0; f < N; f++)
        avg += vid_psnr(frames + (size_t)f * npix, dec_rec + (size_t)f * npix, npix);
    avg /= N;
    printf("  info: video closed-loop last_psnr=%.2f avg_psnr=%.2f dB\n", psnr, avg);
    report("video_avg_psnr_db", avg, ">=", 30.0, avg >= 30.0);

    /* dump the decoded PPM SEQUENCE (replay artifacts) */
    for (int f = 0; f < N; f++) {
        uint8_t *rgb = malloc((size_t)W * H * 3);
        if (!rgb) break;
        const uint8_t *src = dec_rec + (size_t)f * npix;
        for (int i = 0; i < npix; i++)
            rgb[3 * i] = rgb[3 * i + 1] = rgb[3 * i + 2] = src[i];
        char path[64];
        snprintf(path, sizeof(path), "out/video_%02d.ppm", f);
        ppm_write(path, rgb, W, H);
        free(rgb);
    }

    free(frames); free(enc_rec); free(dec_rec); free(pkts); free(lens);
    return avg >= 30.0 ? 0 : -1;
}

/* ---------- A/V sync (module copied from lab18): audio master clock ----------
 * Simulated shared channel: audio plays content M at wall M + L; video frame
 * k (content 33.33k ms) arrives at wall 33k + d_k + D. The sync module drops
 * frames that arrive after the master passed their content time + tolerance. */
static int av_sync_run(int video_delay_ms, int *out_dropped, int *out_nv) {
    const int na = 60, nv = 18;
    const double L = 90.0; /* audio playout latency: mean delay 50 + JB target 40 */
    unsigned rng = 7u;
    int64_t a_arr[60];
    for (int i = 0; i < na; i++) {
        rng = rng * 1103515245u + 12345u;
        int delay = (int)((rng >> 16) & 0x7fff) % 100;
        rng = rng * 1103515245u + 12345u;
        if ((int)((rng >> 16) & 0x7fff) % 100 < 10) { a_arr[i] = -1; continue; }
        a_arr[i] = (int64_t)i * 20 + delay;
    }
    unsigned rv = 9u;
    int64_t v_arr[18];
    for (int i = 0; i < nv; i++) {
        rv = rv * 1103515245u + 12345u;
        int delay = (int)((rv >> 16) & 0x7fff) % 100;
        rv = rv * 1103515245u + 12345u;
        if ((int)((rv >> 16) & 0x7fff) % 100 < 10) { v_arr[i] = -1; continue; }
        v_arr[i] = (int64_t)i * 33 + delay + video_delay_ms;
    }
    avsync_lite s;
    avsync_lite_init(&s, 33.0);
    int ai = 0, vi = 0, dropped = 0;
    double max_display_late = 0;
    while (ai < na || vi < nv) {
        int64_t ta = (ai < na && a_arr[ai] >= 0) ? a_arr[ai] : INT64_MAX;
        int64_t tv = (vi < nv && v_arr[vi] >= 0) ? v_arr[vi] : INT64_MAX;
        if (ai < na && ta <= tv) {
            ai++;
            continue;
        }
        if (tv < INT64_MAX) { /* lost video frames never reach the sync module */
            double audio_master = (double)tv - L;
            double video_ms = (double)vi * 33.333;
            double due_in;
            int dec = avsync_lite_frame(&s, video_ms, audio_master, &due_in);
            if (dec == 0) dropped++;
            if (dec == 1) {
                double late = -due_in;
                if (late > max_display_late) max_display_late = late;
            }
        }
        vi++;
    }
    *out_dropped = dropped;
    *out_nv = nv;
    printf("  info: A/V sync D=%dms -> displayed=%d dropped=%d skew=%.1fms max_display_late=%.1fms\n",
           video_delay_ms, s.displayed, s.dropped, s.skew_ema_ms, max_display_late);
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
    report("av_sync_drops_on_video_lag", (double)droppedD, ">=", 1.0, okD && droppedD >= 1);
    return (ok0 && okD && dropped0 == 0 && droppedD >= 1) ? 0 : -1;
}

static int run_selftest(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== lab19 selftest ===\n");
    if (test_full_chain() != 0) g_fail++;
    if (test_agc_integration() != 0) g_fail++;
    if (test_probe_localizer() != 0) g_fail++;
    if (test_video_line() != 0) g_fail++;
    if (test_av_sync() != 0) g_fail++;
    if (test_bounded_vs_unbounded() != 0) g_fail++;
    printf("summary: %d passed, %d failed\n", g_pass, g_fail);
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
