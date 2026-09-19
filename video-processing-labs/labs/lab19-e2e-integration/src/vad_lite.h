#ifndef LAB19_VAD_LITE_H
#define LAB19_VAD_LITE_H

/* copied from lab06 (energy VAD idea), frame-level simplified for the chain:
 * speech = frame energy above the adaptive noise floor (fast-down / slow-up
 * EMA so speech does not raise the floor) with hangover. */

typedef struct {
    double floor_pow; /* adaptive noise power floor */
    double thr_db;    /* speech threshold above the floor */
    int hangover;     /* frames to hold speech after trigger */
    int hold;
} vad_lite;

void vad_lite_init(vad_lite *v, double thr_db, int hangover);
/* returns 1 while speech (including hangover) */
int vad_lite_frame(vad_lite *v, const double *x, int n);

#endif
