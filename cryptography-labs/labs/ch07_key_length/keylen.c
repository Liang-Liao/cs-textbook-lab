#include "keylen.h"
#include "crypto_common.h"
#include <math.h>
#include <float.h>

double brute_force_years(int key_bits, double keys_per_sec)
{
    /* 2^bits / keys_per_sec seconds → years.
     * Returns -1 for bad input, 1e300 when the true value overflows double
     * (either 2^bits itself or the division by a tiny keys_per_sec). */
    const double SEC_PER_YEAR = 365.25 * 24 * 3600.0;
    double seconds;
    if (key_bits < 0 || keys_per_sec <= 0) return -1;
    if (key_bits > 1023) return 1e300; /* 2^1024 already exceeds DBL_MAX */
    seconds = ldexp(1.0, key_bits) / keys_per_sec;
    if (seconds > DBL_MAX / SEC_PER_YEAR) return 1e300;
    return seconds / SEC_PER_YEAR;
}

double birthday_trials(double block_bits)
{
    return ldexp(1.0, (int)(block_bits / 2.0)); /* ≈ 2^{b/2} */
}

static void enc8(uint64_t k, const uint8_t in[8], uint8_t out[8])
{
    /* Toy block: add the low key byte to every position (repeat-key). */
    int i;
    uint8_t kb = (uint8_t)k;
    for (i = 0; i < 8; i++)
        out[i] = (uint8_t)(in[i] + kb);
}

int count_matching_keys(uint8_t pt[8], uint8_t ct[8], int *out_count)
{
    int k, c = 0;
    for (k = 0; k < 256; k++) {
        uint8_t out[8];
        uint64_t kk = (uint64_t)k;
        enc8(kk, pt, out);
        if (memcmp(out, ct, 8) == 0) c++;
    }
    *out_count = c;
    return 0;
}
