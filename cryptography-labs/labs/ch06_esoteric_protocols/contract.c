#include "contract.h"
#include "sha256.h"
#include "crypto_common.h"

/* Domain-separated, length-framed commitment (same scheme as ch05):
 * c = H(plen || party || tlen || text || nlen || nonce), lengths as 8-byte LE.
 * Without the framing, ("pay $10","1") and ("pay $1","01") hash identically
 * and one commitment could open as two different contracts. */
void contract_commit(const char *party,
                     const uint8_t *text, size_t text_len,
                     const uint8_t *nonce, size_t nonce_len,
                     contract_commit_t *out)
{
    sha256_ctx_t ctx;
    size_t plen = strlen(party);
    uint8_t lenbuf[24];
    size_t i;
    for (i = 0; i < 8; i++) {
        lenbuf[i]      = (uint8_t)((uint64_t)plen >> (8 * i));
        lenbuf[8 + i]  = (uint8_t)((uint64_t)text_len >> (8 * i));
        lenbuf[16 + i] = (uint8_t)((uint64_t)nonce_len >> (8 * i));
    }
    sha256_init(&ctx);
    sha256_update(&ctx, lenbuf, sizeof(lenbuf));
    sha256_update(&ctx, (const uint8_t *)party, plen);
    sha256_update(&ctx, text, text_len);
    sha256_update(&ctx, nonce, nonce_len);
    sha256_final(&ctx, out->digest);
    out->committed = 1;
}

int contract_open_check(const char *party,
                        const uint8_t *text, size_t text_len,
                        const uint8_t *nonce, size_t nonce_len,
                        const contract_commit_t *c)
{
    contract_commit_t t;
    if (!c || !c->committed) return 0;
    contract_commit(party, text, text_len, nonce, nonce_len, &t);
    return memeq_const(t.digest, c->digest, CONTRACT_DIGEST);
}

void contract_session_init(contract_session_t *s)
{
    memset(s, 0, sizeof(*s));
}

static int store_blob(uint8_t *dst, size_t cap, size_t *out_len,
                      const uint8_t *src, size_t len)
{
    if (len > cap) return -1;
    memcpy(dst, src, len);
    *out_len = len;
    return 0;
}

int contract_exchange_commit(contract_session_t *s,
                             const uint8_t *a_text, size_t a_text_len,
                             const uint8_t *a_nonce, size_t a_nonce_len,
                             const uint8_t *b_text, size_t b_text_len,
                             const uint8_t *b_nonce, size_t b_nonce_len)
{
    if (store_blob(s->a_text, sizeof(s->a_text), &s->a_text_len, a_text, a_text_len) != 0)
        return -1;
    if (store_blob(s->a_nonce, sizeof(s->a_nonce), &s->a_nonce_len, a_nonce, a_nonce_len) != 0)
        return -1;
    if (store_blob(s->b_text, sizeof(s->b_text), &s->b_text_len, b_text, b_text_len) != 0)
        return -1;
    if (store_blob(s->b_nonce, sizeof(s->b_nonce), &s->b_nonce_len, b_nonce, b_nonce_len) != 0)
        return -1;
    contract_commit("Alice", a_text, a_text_len, a_nonce, a_nonce_len, &s->a_commit);
    contract_commit("Bob",   b_text, b_text_len, b_nonce, b_nonce_len, &s->b_commit);
    s->a_opened = 0;
    s->b_opened = 0;
    return 0;
}

int contract_exchange_open(contract_session_t *s, char party)
{
    if (party == 'A') {
        if (!contract_open_check("Alice", s->a_text, s->a_text_len,
                                 s->a_nonce, s->a_nonce_len, &s->a_commit))
            return -1;
        s->a_opened = 1;
        return 0;
    }
    if (party == 'B') {
        if (!contract_open_check("Bob", s->b_text, s->b_text_len,
                                 s->b_nonce, s->b_nonce_len, &s->b_commit))
            return -1;
        s->b_opened = 1;
        return 0;
    }
    return -1;
}

int contract_exchange_complete(const contract_session_t *s)
{
    return s->a_opened && s->b_opened &&
           contract_open_check("Alice", s->a_text, s->a_text_len,
                               s->a_nonce, s->a_nonce_len, &s->a_commit) &&
           contract_open_check("Bob", s->b_text, s->b_text_len,
                               s->b_nonce, s->b_nonce_len, &s->b_commit);
}

int contract_prove_bound(const contract_session_t *s, char party,
                         const uint8_t *text, size_t text_len,
                         const uint8_t *nonce, size_t nonce_len)
{
    if (party == 'A')
        return contract_open_check("Alice", text, text_len, nonce, nonce_len, &s->a_commit);
    if (party == 'B')
        return contract_open_check("Bob", text, text_len, nonce, nonce_len, &s->b_commit);
    return 0;
}
