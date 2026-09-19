#ifndef LAB19_CHANNEL_LITE_H
#define LAB19_CHANNEL_LITE_H
#include <stdint.h>

/* lab18-inspired (relay delay/loss/reorder heap), media-time in-process trim:
 * no threads, no wall clock — packets carry a media send time, the channel
 * draws a random delay, may drop or add reorder delay; ch_recv pops packets
 * whose due time has passed in media time. */

typedef struct {
    int due_ms;
    int seq;
    uint8_t *data;
    int len;
} ch_item;

typedef struct {
    ch_item *a;
    int n, cap;
    double loss_rate;
    int reorder_pct;
    int max_delay_ms;
    unsigned rng;
    int sent, dropped, delivered;
} channel;

void ch_init(channel *c, int cap, double loss_rate, int max_delay_ms,
             int reorder_pct, unsigned seed);
void ch_free(channel *c);
/* send a packet at media time now_ms (payload copied) */
int ch_send(channel *c, int now_ms, int seq, const uint8_t *data, int len);
/* pop the next packet due by media time now_ms; 0 = got one, -1 = none */
int ch_recv(channel *c, int now_ms, int *seq, uint8_t *buf, int cap, int *len);

#endif
