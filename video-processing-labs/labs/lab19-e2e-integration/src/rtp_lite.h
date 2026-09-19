#ifndef LAB19_RTP_LITE_H
#define LAB19_RTP_LITE_H
#include <stdint.h>

typedef struct {
    uint16_t seq;
    uint32_t ts;
    uint8_t pt;
    uint8_t *payload;
    int len;
} pkt;

int pkt_pack(const pkt *p, uint8_t *buf, int cap);
int pkt_unpack(const uint8_t *buf, int n, pkt *p);

/* simple seq-ordered jitter buffer */
typedef struct {
    pkt *slots;
    int cap, count;
} jb_lite;

void jb_init(jb_lite *b, int cap);
void jb_free(jb_lite *b);
int jb_insert(jb_lite *b, pkt p); /* copies header; payload borrowed */
int jb_take(jb_lite *b, uint16_t seq, pkt *out);
int jb_has(const jb_lite *b, uint16_t seq);
#endif
