#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mp4.h"

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

typedef struct {
    int nframes;
    int base_size;
    int size_step;
    uint32_t timescale;
    const char *path;
} mp4_variant;

static int verify_variant(const mp4_variant *v, uint32_t *sizes, uint8_t *payload, int ns) {
    if (mp4_write_minimal(v->path, payload, ns, sizes, v->nframes, v->timescale,
                          0) != 0)
        return -1;

    box_info boxes[128];
    int nb = 0;
    if (mp4_parse_boxes(v->path, boxes, 128, &nb) != 0) return -1;
    int has_ftyp = 0, has_moov = 0, has_mdat = 0, has_stsz = 0, has_stco = 0, has_stts = 0;
    int has_trak = 0, max_depth = 0;
    for (int i = 0; i < nb; i++) {
        if (boxes[i].depth > max_depth) max_depth = boxes[i].depth;
        if (!strcmp(boxes[i].type, "ftyp")) has_ftyp = 1;
        if (!strcmp(boxes[i].type, "moov")) has_moov = 1;
        if (!strcmp(boxes[i].type, "mdat")) has_mdat = 1;
        if (!strcmp(boxes[i].type, "stsz")) has_stsz = 1;
        if (!strcmp(boxes[i].type, "stco")) has_stco = 1;
        if (!strcmp(boxes[i].type, "stts")) has_stts = 1;
        if (!strcmp(boxes[i].type, "trak")) has_trak = 1;
    }
    int ok_boxes = has_ftyp && has_moov && has_mdat && has_stsz && has_stco && has_stts && has_trak;
    /* recursive walk must reach stbl children (depth >= 4) */
    int ok_depth = max_depth >= 4;

    uint32_t offs[256], ssz[256], tts[256];
    int n = 0;
    if (mp4_sample_table(v->path, offs, ssz, tts, 256, &n) != 0) return -1;
    int ok_n = (n == v->nframes);
    int ok_sizes = 1;
    for (int i = 0; i < n; i++)
        if (ssz[i] != sizes[i]) ok_sizes = 0;

    int ok_rand = 1;
    FILE *f = fopen(v->path, "rb");
    if (!f) ok_rand = 0;
    for (int i = 0; i < n && f; i++) {
        uint8_t tmp[512];
        if (ssz[i] > sizeof(tmp)) {
            ok_rand = 0;
            break;
        }
        if (fseek(f, (long)offs[i], SEEK_SET) != 0) {
            ok_rand = 0;
            break;
        }
        if (fread(tmp, 1, ssz[i], f) != ssz[i]) {
            ok_rand = 0;
            break;
        }
        int base = 0;
        for (int k = 0; k < i; k++) base += (int)sizes[k];
        for (uint32_t k = 0; k < ssz[i]; k++) {
            if (tmp[k] != payload[base + (int)k]) {
                ok_rand = 0;
                break;
            }
        }
    }
    if (f) fclose(f);

    printf("  info: %s frames=%d boxes=%d max_depth=%d sample_n=%d rand=%d\n", v->path,
           v->nframes, nb, max_depth, n, ok_rand);
    return (ok_boxes && ok_depth && ok_n && ok_sizes && ok_rand) ? 0 : -1;
}

static int test_mp4_variants(void) {
    uint32_t sizes[256];
    uint8_t payload[8192];
    const mp4_variant vars[3] = {
        {8, 32, 8, 1000, "out/v0_tiny.mp4"},
        {16, 48, 4, 30000, "out/v1_mid.mp4"},
        {24, 16, 6, 90000, "out/v2_long.mp4"},
    };
    int all_ok = 1;
    for (int v = 0; v < 3; v++) {
        int ns = 0;
        for (int i = 0; i < vars[v].nframes; i++) {
            sizes[i] = (uint32_t)(vars[v].base_size + i * vars[v].size_step);
            for (uint32_t k = 0; k < sizes[i] && ns < (int)sizeof(payload); k++)
                payload[ns++] = (uint8_t)(v * 40 + i * 17 + k);
        }
        if (verify_variant(&vars[v], sizes, payload, ns) != 0) all_ok = 0;
    }
    report("mp4_variants_ok", all_ok ? 3.0 : 0.0, "==", 3.0, all_ok);
    return all_ok ? 0 : -1;
}

static int test_pts_dts_heap(void) {
    /* I0 P3 B1 B2 P4 P5:
       display (PTS) order = 0,1,2,3,4,5
       decode  (DTS) order = 0,3,1,2,4,5  (B frames after their refs) */
    const int n = 6;
    const int decode_order[6] = {0, 3, 1, 2, 4, 5};
    av_frame_ts frames[6];
    for (int i = 0; i < n; i++) {
        frames[i].id = i;
        frames[i].pts = i; /* display order equals id */
    }
    for (int di = 0; di < n; di++) frames[decode_order[di]].dts = di;

    const int feed[6] = {3, 0, 5, 2, 1, 4}; /* shuffled network order */
    av_frame_ts shuffled[6];
    for (int i = 0; i < n; i++) shuffled[i] = frames[feed[i]];

    int dec[6], disp[6];
    if (pts_dts_reorder(shuffled, n, dec, disp) != 0) return -1;

    int ok_dec = 1;
    for (int i = 0; i < n; i++)
        if (dec[i] != decode_order[i]) ok_dec = 0;
    int ok_disp = 1;
    for (int i = 0; i < n; i++)
        if (disp[i] != i) ok_disp = 0;

    printf("  info: decode order:");
    for (int i = 0; i < n; i++) printf(" %d", dec[i]);
    printf(" | display:");
    for (int i = 0; i < n; i++) printf(" %d", disp[i]);
    printf("\n");

    report("pts_dts_decode_order", ok_dec ? 1.0 : 0.0, "==", 1.0, ok_dec);
    report("pts_dts_display_order", ok_disp ? 1.0 : 0.0, "==", 1.0, ok_disp);
    return (ok_dec && ok_disp) ? 0 : -1;
}

/* Roadmap exp 2 (second half): I/P/B frame size statistics + stss keyframe
 * index. Build a stream with keyframes every 4th sample, mux with an stss
 * table, parse it back and report the per-class size distribution. */
static int test_ipb_stats(void) {
    const int nframes = 24;
    uint32_t sizes[24];
    uint8_t payload[16384];
    int ns = 0;
    /* keyframes large, P frames medium, B frames small */
    for (int i = 0; i < nframes; i++) {
        int cls = (i % 4 == 0) ? 0 : (i % 4 == 1 ? 1 : 2); /* I / P / B */
        sizes[i] = (uint32_t)(cls == 0 ? 400 + i : (cls == 1 ? 200 + i : 60 + i));
        for (uint32_t k = 0; k < sizes[i] && ns < (int)sizeof(payload); k++)
            payload[ns++] = (uint8_t)(i * 7 + k);
    }
    if (mp4_write_minimal("out/ipb.mp4", payload, ns, sizes, nframes, 90000, 4) != 0)
        return -1;

    uint32_t sync[32];
    int nkeys = 0;
    if (mp4_sync_samples("out/ipb.mp4", sync, 32, &nkeys) != 0) {
        report("stss_parse", 0.0, "==", 1.0, 0);
        return -1;
    }
    int ok_keys = (nkeys == 6);
    for (int i = 0; i < nkeys && ok_keys; i++)
        if (sync[i] != (uint32_t)(4 * i + 1)) ok_keys = 0; /* 1-based: 1,5,9,13,17,21 */
    printf("  info: stss keys=%d:", nkeys);
    for (int i = 0; i < nkeys; i++) printf(" %u", sync[i]);
    printf("\n");
    report("stss_parse", ok_keys ? 1.0 : 0.0, "==", 1.0, ok_keys);

    /* size stats per class */
    double sum[3] = {0, 0, 0};
    int cnt[3] = {0, 0, 0}, mn[3] = {1 << 30, 1 << 30, 1 << 30}, mx[3] = {0, 0, 0};
    for (int i = 0; i < nframes; i++) {
        int cls = (i % 4 == 0) ? 0 : (i % 4 == 1 ? 1 : 2);
        sum[cls] += sizes[i];
        cnt[cls]++;
        if ((int)sizes[i] < mn[cls]) mn[cls] = (int)sizes[i];
        if ((int)sizes[i] > mx[cls]) mx[cls] = (int)sizes[i];
    }
    const char *names[3] = {"I", "P", "B"};
    for (int c = 0; c < 3; c++)
        printf("  info: %s frames: n=%d mean=%.1f min=%d max=%d\n", names[c], cnt[c],
               cnt[c] ? sum[c] / cnt[c] : 0.0, mn[c], mx[c]);
    int ok_stats = cnt[0] == 6 && cnt[1] == 6 && cnt[2] == 12 &&
                   sum[0] / 6 > sum[1] / 6 && sum[1] / 6 > sum[2] / 12;
    report("ipb_size_stats", ok_stats ? 1.0 : 0.0, "==", 1.0, ok_stats);
    return (ok_keys && ok_stats) ? 0 : -1;
}

static int run_selftest(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== lab17 selftest ===\n");
    if (test_mp4_variants() != 0) g_fail++;
    if (test_ipb_stats() != 0) g_fail++;
    if (test_pts_dts_heap() != 0) g_fail++;
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
