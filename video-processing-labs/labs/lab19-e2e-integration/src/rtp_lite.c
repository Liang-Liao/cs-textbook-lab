/* copied from lab18-rtp-transport src/rtp.c (rtp_pack/rtp_unpack), trimmed:
 * send_ms extension dropped, SSRC/marker fields not carried in the lite
 * header (the E2E chain only needs seq/ts/pt for ordering and jitter). */
#include "rtp_lite.h"
#include <stdlib.h>
#include <string.h>

int pkt_pack(const pkt *p, uint8_t *buf, int cap) {
    if (!p || !buf || cap < 12 + p->len) return -1;
    buf[0] = 0x80;
    buf[1] = p->pt & 0x7f;
    buf[2] = (uint8_t)(p->seq >> 8);
    buf[3] = (uint8_t)(p->seq & 0xff);
    buf[4] = (uint8_t)(p->ts >> 24);
    buf[5] = (uint8_t)(p->ts >> 16);
    buf[6] = (uint8_t)(p->ts >> 8);
    buf[7] = (uint8_t)(p->ts);
    buf[8] = buf[9] = buf[10] = buf[11] = 0;
    if (p->len > 0 && p->payload) memcpy(buf + 12, p->payload, (size_t)p->len);
    return 12 + p->len;
}

int pkt_unpack(const uint8_t *buf, int n, pkt *p) {
    if (!buf || !p || n < 12) return -1;
    if ((buf[0] >> 6) != 2) return -1;
    p->pt = buf[1] & 0x7f;
    p->seq = (uint16_t)((buf[2] << 8) | buf[3]);
    p->ts = ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) | ((uint32_t)buf[6] << 8) | buf[7];
    p->len = n - 12;
    p->payload = (uint8_t *)buf + 12;
    return 0;
}

void jb_init(jb_lite *b, int cap) {
    b->cap = cap > 0 ? cap : 32;
    b->count = 0;
    b->slots = calloc((size_t)b->cap, sizeof(pkt));
}

void jb_free(jb_lite *b) {
    free(b->slots);
    b->slots = NULL;
    b->count = 0;
}

int jb_insert(jb_lite *b, pkt p) {
    if (!b->slots || b->count >= b->cap) return -1;
    for (int i = 0; i < b->cap; i++) {
        if (b->slots[i].payload == NULL) {
            b->slots[i] = p;
            b->count++;
            return 0;
        }
    }
    return -1;
}

int jb_has(const jb_lite *b, uint16_t seq) {
    for (int i = 0; i < b->cap; i++)
        if (b->slots[i].payload != NULL && b->slots[i].seq == seq) return 1;
    return 0;
}

int jb_take(jb_lite *b, uint16_t seq, pkt *out) {
    for (int i = 0; i < b->cap; i++) {
        if (b->slots[i].payload != NULL && b->slots[i].seq == seq) {
            *out = b->slots[i];
            memset(&b->slots[i], 0, sizeof(pkt));
            b->count--;
            return 0;
        }
    }
    return -1;
}
