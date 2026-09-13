#include "crypto_common.h"
#include "keylen.h"
#include <math.h>

int main(void)
{
    test_stats_t s;
    test_begin(&s, "Chapter 7: Key Length");

    test_check(&s, "56-bit at 1e9/s > 1 year",
               brute_force_years(56, 1e9) > 1.0);
    test_check(&s, "128-bit at 1e12/s huge",
               brute_force_years(128, 1e12) > 1e18);
    test_check(&s, "birthday 64-bit ~ 2^32",
               fabs(birthday_trials(64) - 4294967296.0) < 1.0);

    {
        uint8_t pt[8] = {1,2,3,4,5,6,7,8};
        uint8_t ct[8];
        int c = 0;
        int i;
        for (i = 0; i < 8; i++) ct[i] = (uint8_t)(pt[i] + 0x10);
        count_matching_keys(pt, ct, &c);
        test_check(&s, "toy search finds >=1", c >= 1);
    }

    return test_end(&s);
}
