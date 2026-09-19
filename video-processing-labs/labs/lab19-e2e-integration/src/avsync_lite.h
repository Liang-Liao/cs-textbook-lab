#ifndef LAB19_AVSYNC_LITE_H
#define LAB19_AVSYNC_LITE_H

/* copied from lab18 src/rtp.c (avsync), trimmed */

/* Audio-master A/V sync. Both clocks are in MEDIA milliseconds. A video
 * frame is late by (arrival_wall - L) - video_ms against the master; if the
 * master already moved past its content time + tol → drop (stale). */
typedef struct {
    double tol_ms;      /* late tolerance (≈1 video frame) */
    double skew_ema_ms; /* estimated arrival skew (late positive) */
    int displayed, dropped, held;
} avsync_lite;

void avsync_lite_init(avsync_lite *s, double tol_ms);
/* video_ms: frame content time; audio_master_ms: audio playout position.
 * Returns 1 display, 0 drop (late beyond tolerance), -1 hold (way early). */
int avsync_lite_frame(avsync_lite *s, double video_ms, double audio_master_ms,
                      double *due_in_ms);

#endif
