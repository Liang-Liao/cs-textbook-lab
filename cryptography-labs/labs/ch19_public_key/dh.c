#include "dh.h"
#include "crypto_common.h"

void dh_default_params(dh_params_t *p)
{
    /* RFC 3526 768-bit is too big for uint64; use a textbook demo prime.
     * p = 2^31 - 1 is Mersenne prime 2147483647;
     * p-1 = 2 * 3^2 * 7 * 11 * 31 * 151 * 331, and g = 7 is its smallest
     * primitive root (g = 5 is NOT: ord(5) = (p-1)/11). */
    p->p = 2147483647ULL;
    p->g = 7;
}

int dh_derive_public(dh_party_t *party, const dh_params_t *params)
{
    uint64_t rnd = 0;
    uint8_t b[8];
    if (crypto_random_bytes(b, 8) != 0) return -1;
    memcpy(&rnd, b, 8);
    party->priv = 2 + (rnd % (params->p - 3));
    party->pub = mod_pow_u64(params->g, party->priv, params->p);
    return 0;
}

uint64_t dh_shared_secret(uint64_t peer_pub, uint64_t own_priv, const dh_params_t *params)
{
    return mod_pow_u64(peer_pub, own_priv, params->p);
}
