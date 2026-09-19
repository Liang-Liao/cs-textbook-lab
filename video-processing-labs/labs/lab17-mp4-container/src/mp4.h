#ifndef LAB17_MP4_H
#define LAB17_MP4_H
#include <stdint.h>
#include <stdio.h>

typedef struct {
    char type[5];
    uint32_t size;
    long offset;
    int is_container;
    int depth;
} box_info;

/* Write a minimal ISO-BMFF: ftyp + mdat + moov(trak/mdia/minf/stbl).
 * keyframe_interval > 0 also writes an stss sync-sample table. */
int mp4_write_minimal(const char *path, const uint8_t *samples, int nsamp,
                      const uint32_t *sizes, int nframes, uint32_t timescale,
                      int keyframe_interval);

/* Recursive box-tree walk; fills out[] in depth-first pre-order. */
int mp4_parse_boxes(const char *path, box_info *out, int max, int *count);

/* stsz/stco/stts via tree (not raw 4-byte scan). */
int mp4_sample_table(const char *path, uint32_t *offsets, uint32_t *sizes,
                     uint32_t *ts, int max, int *n);

/* Read the stss sync-sample table (1-based keyframe sample numbers).
 * Returns 0 and *n = entry count; -1 if the file has no stss. */
int mp4_sync_samples(const char *path, uint32_t *out, int cap, int *n);

/* Min-heap reorder of out-of-order DTS/PTS into display (PTS) order. */
typedef struct {
    int64_t dts;
    int64_t pts;
    int id;
} av_frame_ts;

typedef struct {
    av_frame_ts *a;
    int n, cap;
} ts_heap;

void ts_heap_init(ts_heap *h, int cap);
void ts_heap_free(ts_heap *h);
int ts_heap_push_dts(ts_heap *h, av_frame_ts f);
int ts_heap_push_pts(ts_heap *h, av_frame_ts f);
int ts_heap_pop(ts_heap *h, av_frame_ts *out);

/* Feed shuffled frames; pop decode order (DTS) then display order (PTS).
 * Returns 0 on success. display_ids receives id sequence sorted by PTS. */
int pts_dts_reorder(const av_frame_ts *in, int n, int *decode_ids, int *display_ids);

#endif
