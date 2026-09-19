#include "adpcm.h"
#include <math.h>
#include <stdlib.h>

static const int step_tab[89] = {
    7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,
    97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,
    598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,
    2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,
    11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767
};
static const int index_tab[16] = {-1,-1,-1,-1,2,4,6,8,-1,-1,-1,-1,2,4,6,8};

void adpcm_encode(const int16_t *pcm, int n, uint8_t *code) {
    int pred = 0, idx = 0;
    for (int i = 0; i < n; i++) {
        int step = step_tab[idx];
        int diff = pcm[i] - pred;
        int sign = 0;
        if (diff < 0) { sign = 8; diff = -diff; }
        int delta = 0;
        if (diff >= step) { delta |= 4; diff -= step; }
        if (diff >= step >> 1) { delta |= 2; diff -= step >> 1; }
        if (diff >= step >> 2) delta |= 1;
        int q = sign | delta;
        /* decode same as decoder to keep state */
        int diffq = step >> 3;
        if (delta & 4) diffq += step;
        if (delta & 2) diffq += step >> 1;
        if (delta & 1) diffq += step >> 2;
        if (sign) pred -= diffq; else pred += diffq;
        if (pred > 32767) pred = 32767;
        if (pred < -32768) pred = -32768;
        idx += index_tab[q];
        if (idx < 0) idx = 0;
        if (idx > 88) idx = 88;
        if ((i & 1) == 0) code[i/2] = (uint8_t)((q & 15) << 4);
        else code[i/2] |= (uint8_t)(q & 15);
    }
}

void adpcm_decode(const uint8_t *code, int n, int16_t *pcm) {
    int pred = 0, idx = 0;
    for (int i = 0; i < n; i++) {
        int q = (i & 1) ? (code[i/2] & 15) : ((code[i/2] >> 4) & 15);
        int step = step_tab[idx];
        int sign = q & 8, delta = q & 7;
        int diffq = step >> 3;
        if (delta & 4) diffq += step;
        if (delta & 2) diffq += step >> 1;
        if (delta & 1) diffq += step >> 2;
        if (sign) pred -= diffq; else pred += diffq;
        if (pred > 32767) pred = 32767;
        if (pred < -32768) pred = -32768;
        pcm[i] = (int16_t)pred;
        idx += index_tab[q];
        if (idx < 0) idx = 0;
        if (idx > 88) idx = 88;
    }
}

double adpcm_snr_db(const int16_t *ref, const int16_t *dec, int n) {
    double ps = 0, pe = 0;
    for (int i = 0; i < n; i++) {
        double e = (double)ref[i] - dec[i];
        ps += (double)ref[i] * ref[i];
        pe += e * e;
    }
    if (pe <= 0) return 200;
    return 10 * log10(ps / pe);
}
