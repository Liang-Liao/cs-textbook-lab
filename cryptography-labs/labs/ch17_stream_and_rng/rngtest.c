#include "rngtest.h"
#include "crypto_common.h"

double rng_monobit_bias(const uint8_t *buf, size_t len)
{
    size_t bits = len * 8, ones = 0, i, b;
    for (i = 0; i < len; i++)
        for (b = 0; b < 8; b++)
            ones += (buf[i] >> b) & 1;
    if (bits == 0) return 1.0;
    return (double)((ones > bits / 2) ? ones - bits / 2 : bits / 2 - ones) /
           (double)(bits / 2 ? bits / 2 : 1);
}

/* Runs test, FIPS 140-2 style, over the first 20000 bits (2500 bytes).
 * The 0-run and 1-run counts of each length 1..5 must stay inside the
 * official interval, and so must the bucket of runs of length >= 6.
 * Shorter inputs are rejected (the interval table only applies to n=20000).
 * The table is ~4 sigma wide: a truly random source is rejected about 1
 * time in 10^4. */
int rng_runs_ok(const uint8_t *buf, size_t len)
{
    static const long LO[6] = {2315, 1114, 527, 240, 103, 103};
    static const long HI[6] = {2685, 1386, 723, 384, 209, 209};
    long cnt[2][6] = {{0}};
    size_t i, b, k;
    int prev = -1;
    long cur = 0;

    if (len < 20000 / 8) return 0;
    for (i = 0; i < 20000 / 8; i++) {
        for (b = 0; b < 8; b++) {
            int bit = (buf[i] >> b) & 1;
            if (bit == prev) {
                cur++;
            } else {
                if (prev >= 0)
                    cnt[prev][cur >= 6 ? 5 : cur - 1]++;
                prev = bit;
                cur = 1;
            }
        }
    }
    cnt[prev][cur >= 6 ? 5 : cur - 1]++;
    for (k = 0; k < 6; k++) {
        if (cnt[0][k] < LO[k] || cnt[0][k] > HI[k]) return 0;
        if (cnt[1][k] < LO[k] || cnt[1][k] > HI[k]) return 0;
    }
    return 1;
}

double rng_byte_max_dev(const uint8_t *buf, size_t len)
{
    size_t hist[256];
    size_t i;
    double expect, maxd = 0;
    memset(hist, 0, sizeof(hist));
    for (i = 0; i < len; i++) hist[buf[i]]++;
    expect = (double)len / 256.0;
    for (i = 0; i < 256; i++) {
        double d = (double)hist[i] - expect;
        if (d < 0) d = -d;
        if (d > maxd) maxd = d;
    }
    return expect > 0 ? maxd / expect : 1.0;
}
