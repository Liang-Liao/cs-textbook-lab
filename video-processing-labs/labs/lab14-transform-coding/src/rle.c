#include "rle.h"

int rle_encode_block(const int zz[64], int *out, int out_cap) {
    if (!zz || !out || out_cap < 2) return -1;
    int n = 0;
    out[n++] = zz[0]; /* DC */
    int i = 1;
    while (i < 64) {
        int run = 0;
        while (i < 64 && zz[i] == 0) { run++; i++; }
        if (i >= 64) break;
        if (n + 2 >= out_cap) return -1;
        out[n++] = run;
        out[n++] = zz[i];
        i++;
    }
    if (n >= out_cap) return -1;
    out[n++] = RLE_EOB;
    return n;
}

int rle_decode_block(const int *in, int in_n, int zz[64]) {
    if (!in || !zz || in_n < 2) return -1;
    for (int k = 0; k < 64; k++) zz[k] = 0;
    int pos = 0;
    zz[0] = in[pos++];
    int i = 1;
    while (pos < in_n) {
        int s = in[pos];
        if (s == RLE_EOB) { pos++; break; }
        if (pos + 1 >= in_n) return -1;
        int run = in[pos++];
        int level = in[pos++];
        if (run < 0 || i + run >= 64) return -1;
        i += run;
        zz[i++] = level;
    }
    return pos;
}
