#include "crypto_common.h"
#include "rngtest.h"

int main(void)
{
    test_stats_t s;
    uint8_t buf[4096];
    uint8_t allzero[256];

    test_begin(&s, "Chapter 17: Randomness Tests");

    memset(allzero, 0, sizeof(allzero));
    test_check(&s, "all-zero monobit bias high", rng_monobit_bias(allzero, 256) > 0.9);

    if (crypto_random_bytes(buf, sizeof(buf)) == 0) {
        test_check(&s, "CSPRNG monobit bias low", rng_monobit_bias(buf, sizeof(buf)) < 0.05);
        test_check(&s, "CSPRNG runs ok", rng_runs_ok(buf, sizeof(buf)) == 1);
        test_check(&s, "CSPRNG byte hist ok", rng_byte_max_dev(buf, sizeof(buf)) < 2.0);
    } else {
        test_check(&s, "CSPRNG available", 0);
    }

    /* Deterministic weak sources: bit-balanced but broken run structure. */
    {
        uint8_t alt[2500], chunky[2500];
        size_t i;
        for (i = 0; i < sizeof(alt); i++) alt[i] = 0xAA;    /* 1010...: only length-1 runs */
        for (i = 0; i < sizeof(chunky); i++) chunky[i] = (i & 1) ? 0xFF : 0x00;
        test_check(&s, "alternating bits: monobit balanced",
                   rng_monobit_bias(alt, sizeof(alt)) < 0.05);
        test_check(&s, "alternating bits fail runs", rng_runs_ok(alt, sizeof(alt)) == 0);
        test_check(&s, "length-8 runs fail runs test", rng_runs_ok(chunky, sizeof(chunky)) == 0);
        test_check(&s, "short input rejected by runs test", rng_runs_ok(alt, 64) == 0);
    }

    {
        uint8_t weak[64];
        memset(weak, 0, sizeof(weak));
        test_check(&s, "all-zero detectable", rng_monobit_bias(weak, 64) > 0.9);
    }

    return test_end(&s);
}
