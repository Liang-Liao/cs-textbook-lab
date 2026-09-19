/* lab17 MP4: recursive box tree + sample tables + PTS/DTS min-heap */
#include "mp4.h"
#include <stdlib.h>
#include <string.h>

static void wbe32(FILE *f, uint32_t v) {
    uint8_t b[4] = {(uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v};
    fwrite(b, 1, 4, f);
}
static void wtag(FILE *f, const char *t) { fwrite(t, 1, 4, f); }

static uint32_t rbe32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

int mp4_write_minimal(const char *path, const uint8_t *samples, int nsamp,
                      const uint32_t *sizes, int nframes, uint32_t timescale,
                      int keyframe_interval) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    wbe32(f, 20);
    wtag(f, "ftyp");
    wtag(f, "isom");
    wbe32(f, 0);
    wtag(f, "isom");
    wbe32(f, 8 + (uint32_t)nsamp);
    wtag(f, "mdat");
    long data_off = ftell(f);
    fwrite(samples, 1, (size_t)nsamp, f);

    long moov = ftell(f);
    wbe32(f, 0);
    wtag(f, "moov");
    long trak = ftell(f);
    wbe32(f, 0);
    wtag(f, "trak");
    long mdia = ftell(f);
    wbe32(f, 0);
    wtag(f, "mdia");
    wbe32(f, 32);
    wtag(f, "mdhd");
    wbe32(f, 0); /* version + flags */
    wbe32(f, 0); /* creation */
    wbe32(f, 0); /* modification */
    wbe32(f, timescale);
    wbe32(f, (uint32_t)nframes); /* duration */
    wbe32(f, 0x55c40000); /* language + quality packed */
    long minf = ftell(f);
    wbe32(f, 0);
    wtag(f, "minf");
    long stbl = ftell(f);
    wbe32(f, 0);
    wtag(f, "stbl");
    wbe32(f, 20 + 4 * (uint32_t)nframes);
    wtag(f, "stsz");
    wbe32(f, 0);
    wbe32(f, 0);
    wbe32(f, (uint32_t)nframes);
    for (int i = 0; i < nframes; i++) wbe32(f, sizes[i]);
    wbe32(f, 20);
    wtag(f, "stco");
    wbe32(f, 0);
    wbe32(f, 1);
    wbe32(f, (uint32_t)data_off);
    wbe32(f, 24);
    wtag(f, "stts");
    wbe32(f, 0);
    wbe32(f, 1);
    wbe32(f, (uint32_t)nframes);
    wbe32(f, 1);
    /* stss: sync (keyframe) sample numbers, 1-based */
    int nkeys = 0;
    if (keyframe_interval > 0)
        for (int i = 0; i < nframes; i++)
            if (i % keyframe_interval == 0) nkeys++;
    if (nkeys > 0) {
        wbe32(f, (uint32_t)(16 + 4 * (uint32_t)nkeys));
        wtag(f, "stss");
        wbe32(f, 0);
        wbe32(f, (uint32_t)nkeys);
        for (int i = 0; i < nframes; i++)
            if (i % keyframe_interval == 0) wbe32(f, (uint32_t)(i + 1));
    }
    long end = ftell(f);
    uint32_t sz;
    sz = (uint32_t)(end - stbl);
    fseek(f, stbl, SEEK_SET);
    wbe32(f, sz);
    sz = (uint32_t)(end - minf);
    fseek(f, minf, SEEK_SET);
    wbe32(f, sz);
    sz = (uint32_t)(end - mdia);
    fseek(f, mdia, SEEK_SET);
    wbe32(f, sz);
    sz = (uint32_t)(end - trak);
    fseek(f, trak, SEEK_SET);
    wbe32(f, sz);
    sz = (uint32_t)(end - moov);
    fseek(f, moov, SEEK_SET);
    wbe32(f, sz);
    fclose(f);
    return 0;
}

static int is_container(const char *type) {
    static const char *c[] = {"moov", "trak", "mdia", "minf", "stbl", "edts",
                              "dinf", "udta", "mvex", "moof", "traf", "skip", NULL};
    for (int i = 0; c[i]; i++)
        if (memcmp(type, c[i], 4) == 0) return 1;
    return 0;
}

static int printable4(const uint8_t *p) {
    for (int k = 0; k < 4; k++)
        if (p[k] < 0x20 || p[k] > 0x7e) return 0;
    return 1;
}

typedef struct {
    box_info *out;
    int max;
    int n;
} walk_ctx;

static void walk_range(const uint8_t *buf, long start, long end, int depth, walk_ctx *ctx);

static void walk_box(const uint8_t *buf, long off, long end, int depth, walk_ctx *ctx) {
    if (ctx->n >= ctx->max) return;
    if (off + 8 > end) return;
    uint32_t sz = rbe32(buf + off);
    const uint8_t *tp = buf + off + 4;
    if (!printable4(tp)) return;
    if (sz < 8) return;
    if (sz == 1) return; /* 64-bit size not used by our muxer */
    if (off + (long)sz > end) return;

    box_info *b = &ctx->out[ctx->n++];
    memcpy(b->type, tp, 4);
    b->type[4] = 0;
    b->size = sz;
    b->offset = off;
    b->is_container = is_container(b->type);
    b->depth = depth;

    if (b->is_container && depth < 8)
        walk_range(buf, off + 8, off + (long)sz, depth + 1, ctx);
}

static void walk_range(const uint8_t *buf, long start, long end, int depth, walk_ctx *ctx) {
    long p = start;
    while (p + 8 <= end && ctx->n < ctx->max) {
        uint32_t sz = rbe32(buf + p);
        if (sz < 8 || p + (long)sz > end) break;
        walk_box(buf, p, end, depth, ctx);
        p += (long)sz;
    }
}

static uint8_t *read_all(const char *path, long *fsz) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *fsz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (*fsz <= 0) {
        fclose(f);
        return NULL;
    }
    uint8_t *buf = malloc((size_t)*fsz);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    if (fread(buf, 1, (size_t)*fsz, f) != (size_t)*fsz) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    return buf;
}

int mp4_parse_boxes(const char *path, box_info *out, int max, int *count) {
    long fsz = 0;
    uint8_t *buf = read_all(path, &fsz);
    if (!buf) return -1;
    walk_ctx ctx = {out, max, 0};
    walk_range(buf, 0, fsz, 0, &ctx);
    free(buf);
    if (count) *count = ctx.n;
    return 0;
}

/* Find first box of type at/under start..end (tree walk). */
static const uint8_t *find_box(const uint8_t *buf, long start, long end, const char *type) {
    long p = start;
    while (p + 8 <= end) {
        uint32_t sz = rbe32(buf + p);
        if (sz < 8 || p + (long)sz > end) return NULL;
        if (memcmp(buf + p + 4, type, 4) == 0) return buf + p;
        if (is_container((const char *)(buf + p + 4))) {
            const uint8_t *r = find_box(buf, p + 8, p + (long)sz, type);
            if (r) return r;
        }
        p += (long)sz;
    }
    return NULL;
}

int mp4_sample_table(const char *path, uint32_t *offsets, uint32_t *sizes,
                     uint32_t *ts, int max, int *nout) {
    long fsz = 0;
    uint8_t *buf = read_all(path, &fsz);
    if (!buf) return -1;

    const uint8_t *stsz = find_box(buf, 0, fsz, "stsz");
    const uint8_t *stco = find_box(buf, 0, fsz, "stco");
    const uint8_t *stts = find_box(buf, 0, fsz, "stts");
    if (!stsz || !stco) {
        free(buf);
        return -1;
    }

    long stsz_off = (long)(stsz - buf);
    uint32_t stsz_box = rbe32(stsz);
    if (stsz_off + 20 > fsz || stsz_box < 20) {
        free(buf);
        return -1;
    }
    uint32_t cnt = rbe32(stsz + 16);
    if ((int)cnt > max) cnt = (uint32_t)max;
    for (uint32_t k = 0; k < cnt; k++) {
        long p = stsz_off + 20 + (long)k * 4;
        if (p + 4 > fsz) {
            cnt = k;
            break;
        }
        sizes[k] = rbe32(stsz + 20 + k * 4);
    }

    long stco_off = (long)(stco - buf);
    if (stco_off + 16 > fsz) {
        free(buf);
        return -1;
    }
    uint32_t nchunk = rbe32(stco + 12);
    uint32_t chunk_off = (nchunk >= 1) ? rbe32(stco + 16) : 0;

    /* stts: expand to per-sample duration → presentation ts */
    uint32_t acc_ts = 0;
    if (stts) {
        long stts_off = (long)(stts - buf);
        uint32_t nent = rbe32(stts + 12);
        uint32_t si = 0;
        for (uint32_t e = 0; e < nent && si < cnt; e++) {
            long p = stts_off + 16 + (long)e * 8;
            if (p + 8 > fsz) break;
            uint32_t sc = rbe32(stts + 16 + e * 8);
            uint32_t sd = rbe32(stts + 16 + e * 8 + 4);
            for (uint32_t j = 0; j < sc && si < cnt; j++, si++) {
                ts[si] = acc_ts;
                acc_ts += sd;
            }
        }
        for (; si < cnt; si++) ts[si] = si;
    } else {
        for (uint32_t k = 0; k < cnt; k++) ts[k] = k;
    }

    uint32_t acc = chunk_off;
    for (uint32_t k = 0; k < cnt; k++) {
        offsets[k] = acc;
        acc += sizes[k];
    }
    free(buf);
    if (nout) *nout = (int)cnt;
    return 0;
}

/* ---- min-heap on DTS or PTS ---- */

void ts_heap_init(ts_heap *h, int cap) {
    h->cap = cap > 0 ? cap : 16;
    h->n = 0;
    h->a = malloc((size_t)h->cap * sizeof(av_frame_ts));
}
void ts_heap_free(ts_heap *h) {
    free(h->a);
    h->a = NULL;
    h->n = h->cap = 0;
}

static int less_dts(const av_frame_ts *x, const av_frame_ts *y) {
    if (x->dts != y->dts) return x->dts < y->dts;
    return x->id < y->id;
}
static int less_pts(const av_frame_ts *x, const av_frame_ts *y) {
    if (x->pts != y->pts) return x->pts < y->pts;
    return x->id < y->id;
}

static int heap_push(ts_heap *h, av_frame_ts f, int by_pts) {
    if (!h->a) return -1;
    if (h->n >= h->cap) {
        int ncap = h->cap * 2;
        av_frame_ts *na = realloc(h->a, (size_t)ncap * sizeof(av_frame_ts));
        if (!na) return -1;
        h->a = na;
        h->cap = ncap;
    }
    int i = h->n++;
    h->a[i] = f;
    while (i > 0) {
        int p = (i - 1) / 2;
        int ok = by_pts ? less_pts(&h->a[i], &h->a[p]) : less_dts(&h->a[i], &h->a[p]);
        if (!ok) break;
        av_frame_ts t = h->a[i];
        h->a[i] = h->a[p];
        h->a[p] = t;
        i = p;
    }
    return 0;
}

int ts_heap_push_dts(ts_heap *h, av_frame_ts f) { return heap_push(h, f, 0); }
int ts_heap_push_pts(ts_heap *h, av_frame_ts f) { return heap_push(h, f, 1); }

static int heap_pop_cmp(ts_heap *h, av_frame_ts *out, int by_pts) {
    if (!h->a || h->n <= 0) return -1;
    *out = h->a[0];
    h->n--;
    if (h->n > 0) {
        h->a[0] = h->a[h->n];
        int i = 0;
        for (;;) {
            int l = 2 * i + 1, r = l + 1, m = i;
            if (l < h->n) {
                int ok = by_pts ? less_pts(&h->a[l], &h->a[m]) : less_dts(&h->a[l], &h->a[m]);
                if (ok) m = l;
            }
            if (r < h->n) {
                int ok = by_pts ? less_pts(&h->a[r], &h->a[m]) : less_dts(&h->a[r], &h->a[m]);
                if (ok) m = r;
            }
            if (m == i) break;
            av_frame_ts t = h->a[i];
            h->a[i] = h->a[m];
            h->a[m] = t;
            i = m;
        }
    }
    return 0;
}

int ts_heap_pop(ts_heap *h, av_frame_ts *out) { return heap_pop_cmp(h, out, 0); }

int pts_dts_reorder(const av_frame_ts *in, int n, int *decode_ids, int *display_ids) {
    if (!in || n <= 0) return -1;
    ts_heap dh, ph;
    ts_heap_init(&dh, n);
    ts_heap_init(&ph, n);
    for (int i = 0; i < n; i++) {
        if (ts_heap_push_dts(&dh, in[i]) != 0) {
            ts_heap_free(&dh);
            ts_heap_free(&ph);
            return -1;
        }
    }
    int nd = 0;
    av_frame_ts tmp;
    while (heap_pop_cmp(&dh, &tmp, 0) == 0) {
        if (decode_ids) decode_ids[nd] = tmp.id;
        ts_heap_push_pts(&ph, tmp);
        nd++;
    }
    int np = 0;
    while (heap_pop_cmp(&ph, &tmp, 1) == 0) {
        if (display_ids) display_ids[np] = tmp.id;
        np++;
    }
    ts_heap_free(&dh);
    ts_heap_free(&ph);
    return (nd == n && np == n) ? 0 : -1;
}

int mp4_sync_samples(const char *path, uint32_t *out, int cap, int *n) {
    if (!out || !n) return -1;
    *n = 0;
    long fsz = 0;
    uint8_t *buf = read_all(path, &fsz);
    if (!buf) return -1;
    const uint8_t *b = find_box(buf, 0, fsz, "stss");
    if (!b) {
        free(buf);
        return -1;
    }
    uint32_t sz = rbe32(b);
    if (sz < 16 || (long)sz > fsz) {
        free(buf);
        return -1;
    }
    uint32_t nkeys = rbe32(b + 12);
    if ((int)nkeys > cap) nkeys = (uint32_t)cap;
    for (uint32_t i = 0; i < nkeys; i++)
        out[i] = rbe32(b + 16 + i * 4);
    *n = (int)nkeys;
    free(buf);
    return 0;
}
