#ifndef LAB18_RTP_H
#define LAB18_RTP_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t seq;
    uint32_t ts;
    uint8_t pt;
    uint8_t marker;
    uint32_t ssrc;
    uint8_t *payload;
    int len;
    int64_t send_ms;
} rtp_pkt;

int rtp_pack(const rtp_pkt *p, uint8_t *buf, int cap);
int rtp_unpack(const uint8_t *buf, int n, rtp_pkt *p);

typedef struct {
    uint16_t seq;
    uint32_t ts;
    uint8_t pt;
    int64_t send_ms;
    int64_t recv_ms;
    uint8_t *payload;
    int len;
    int used;
} ajb_slot;

typedef struct {
    ajb_slot *slots;
    int cap;
    int count;
    int target_ms;
    double delay_ema;
    double jitter;
    int64_t first_recv_ms;
    int min_target_ms;
    int max_target_ms;
} ajb;

void ajb_init(ajb *b, int cap);
void ajb_free(ajb *b);
int ajb_push(ajb *b, const rtp_pkt *p, int64_t now_ms);
int ajb_pop(ajb *b, uint16_t expect_seq, rtp_pkt *out);
int ajb_has(const ajb *b, uint16_t seq);

typedef struct {
    double trend_ms;
    int state;
    double rate;
    int decrease_count;
    int increase_count;
    int loss_events;
} gcc_t;

void gcc_init(gcc_t *g);
/* grad_ms: measured arrival-time gradient; loss_pct: recent loss percentage.
 * Sustained loss > 5% triggers a multiplicative decrease (more aggressive
 * than the gradient's 0.88 factor), per the loss-based side of GCC. */
double gcc_update(gcc_t *g, double grad_ms, double loss_pct);

/* Audio-master A/V sync. Both clocks are in MEDIA milliseconds (audio ts/48
 * for 48 kHz, video ts/90 for 90 kHz). A video frame is due when the audio
 * master reaches video_ms + display_latency; arriving later than tol_ms past
 * that moment means the frame is stale → drop it (resync). Returns 1 display
 * (buffer until due), 0 drop (late beyond tolerance), -1 hold (way early). */
typedef struct {
    double display_latency_ms; /* media lead: display at video_ms + this */
    double tol_ms;             /* late tolerance (≈1 video frame) */
    double skew_ema_ms;        /* estimated arrival skew (late positive) */
    int displayed, dropped, held;
} avsync;

void avsync_init(avsync *s, double display_latency_ms, double tol_ms);
int avsync_frame(avsync *s, double video_ms, double audio_master_ms,
                 double *due_in_ms);

#endif
