#include "avsync_lite.h"

/* copied from lab18-rtp-transport src/rtp.c (avsync_frame), trim */

void avsync_lite_init(avsync_lite *s, double tol_ms) {
    s->tol_ms = tol_ms;
    s->skew_ema_ms = 0.0;
    s->displayed = s->dropped = s->held = 0;
}

int avsync_lite_frame(avsync_lite *s, double video_ms, double audio_master_ms,
                      double *due_in_ms) {
    double due_in = video_ms - audio_master_ms;
    double skew = -due_in;
    s->skew_ema_ms += 0.2 * (skew - s->skew_ema_ms);
    if (due_in_ms) *due_in_ms = due_in;
    if (due_in < -s->tol_ms) {
        s->dropped++;
        return 0;
    }
    if (due_in > 1000.0) {
        s->held++;
        return -1;
    }
    s->displayed++;
    return 1;
}
