#ifndef LAB19_VID_CODEC_H
#define LAB19_VID_CODEC_H
#include <stdint.h>
/* copied/adapted from lab15 (trimmed I/P + packet path for E2E video line) */

void vid_seq_make(uint8_t *img, int w, int h, int t);

/* Encode GOP into packed packets. Each packet: 2-byte len + payload.
 * Returns packet count or -1. rec receives closed-loop reconstructions. */
int vid_encode_gop(const uint8_t *frames, uint8_t *rec, int w, int h, int n,
                   int gop, int qp, uint8_t *pktbuf, int pktcap, int *pkt_lens,
                   int max_pkts);

/* Decode packets into rec. Returns 0 on success. */
int vid_decode_gop(const uint8_t *pktbuf, const int *pkt_lens, int npkts,
                   uint8_t *rec, int w, int h, int n, int gop);

double vid_psnr(const uint8_t *a, const uint8_t *b, int npix);
#endif
