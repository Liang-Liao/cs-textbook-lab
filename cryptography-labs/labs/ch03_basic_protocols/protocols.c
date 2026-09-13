#include "protocols.h"
#include "crypto_common.h"

/* Chapter 3: basic protocols — challenge-response.
 * Uses mix64_u64 as a stand-in one-way function so this lab
 * does not depend on other labs. */

static uint64_t h2(uint64_t s, uint64_t r)
{
    return mix64_u64(mix64_u64(s) ^ mix64_u64(r + 0x9E3779B97F4A7C15ULL));
}

int auth_challenge(uint64_t *out)
{
    uint8_t b[8];
    /* A constant fallback challenge would be replayable; report the failure
     * instead of silently downgrading. */
    if (out == NULL || crypto_random_bytes(b, 8) != 0) return -1;
    memcpy(out, b, 8);
    return 0;
}

uint64_t auth_response(uint64_t secret, uint64_t challenge)
{
    return h2(secret, challenge);
}

int auth_verify(const auth_shared_t *v, uint64_t challenge, uint64_t response)
{
    return auth_response(v->secret, challenge) == response;
}
