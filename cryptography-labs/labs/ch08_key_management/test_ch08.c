#include "crypto_common.h"
#include "keymgr.h"

int main(void)
{
    test_stats_t s;
    km_store_t st;
    uint8_t wrapped[8];
    uint64_t dek = 0;

    test_begin(&s, "Chapter 8: Key Management");

    km_init(&st);
    km_add_key(&st, "KEK-1", KM_KEK, 0x1111, 10000);
    km_add_key(&st, "DEK-1", KM_DEK, 0xABCD, 5000);

    test_check(&s, "wrap ok", km_wrap(&st, "KEK-1", "DEK-1", wrapped, 100) == 0);
    test_check(&s, "unwrap recovers DEK",
               km_unwrap(&st, "KEK-1", wrapped, &dek, 100) == 0 && dek == 0xABCD);

    km_revoke(&st, "DEK-1", 200);
    test_check(&s, "revoked DEK cannot wrap",
               km_wrap(&st, "KEK-1", "DEK-1", wrapped, 200) != 0);

    {
        const km_key_t *k = km_find(&st, "KEK-1");
        test_check(&s, "expired key unusable", km_usable(k, 99999) == 0);
        test_check(&s, "in-date key usable", km_usable(k, 100) == 1);
    }

    /* ID hygiene: over-long ids must be rejected (they would alias via
     * truncation), duplicates must be rejected, distinct ids stay distinct. */
    {
        km_store_t t2;
        km_init(&t2);
        test_check(&s, "over-long id rejected",
                   km_add_key(&t2, "KEK-ALPHA1", KM_KEK, 1, 100) == -1);
        test_check(&s, "8-char id accepted",
                   km_add_key(&t2, "KEK-A1", KM_KEK, 0x1111, 100) == 0);
        test_check(&s, "second distinct id accepted",
                   km_add_key(&t2, "KEK-A2", KM_KEK, 0x2222, 100) == 0);
        test_check(&s, "duplicate id rejected",
                   km_add_key(&t2, "KEK-A1", KM_KEK, 0x3333, 100) == -1);
        test_check(&s, "no aliasing between distinct ids",
                   km_find(&t2, "KEK-A2")->material == 0x2222);
    }

    return test_end(&s);
}
