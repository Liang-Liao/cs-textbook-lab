#include "ticket.h"
#include "crypto_common.h"

static void xor_stream(uint8_t *buf, size_t len, uint64_t key)
{
    size_t i;
    uint64_t s = mix64_u64(key);
    for (i = 0; i < len; i++) {
        if (i % 8 == 0 && i) s = mix64_u64(s + i);
        buf[i] ^= (uint8_t)(s >> (8 * (i % 8)));
    }
}

void ticket_issue(const ticket_plain_t *pt, uint64_t server_key, ticket_t *out)
{
    memcpy(out->enc, pt, sizeof(*pt));
    memset(out->enc + sizeof(*pt), 0, sizeof(out->enc) - sizeof(*pt));
    xor_stream(out->enc, sizeof(*pt), server_key);
}

int ticket_open(const ticket_t *t, uint64_t server_key, ticket_plain_t *out)
{
    uint8_t buf[48];
    memcpy(buf, t->enc, sizeof(buf));
    xor_stream(buf, sizeof(*out), server_key);
    memcpy(out, buf, sizeof(*out));
    return 0;
}

void ts_chain_init(ts_chain_t *c)
{
    c->h = mix64_u64(0x5453434841494E00ULL); /* "TSCHAIN\0" */
    c->seq = 0;
}

static uint64_t ts_mix(uint64_t prev_h, uint64_t seq, uint64_t time,
                       const uint8_t *data, size_t len)
{
    uint64_t h = mix64_u64(prev_h ^ (seq * 0x9E3779B97F4A7C15ULL) ^ time);
    size_t i;
    for (i = 0; i < len; i++)
        h = mix64_u64(h ^ data[i] ^ (i << 3));
    return h;
}

void ts_chain_append(ts_chain_t *c, uint64_t time, const uint8_t *data, size_t len)
{
    c->h = ts_mix(c->h, c->seq, time, data, len);
    c->seq++;
}

int ts_chain_check_link(uint64_t prev_h, uint64_t seq,
                        uint64_t time, const uint8_t *data, size_t len,
                        uint64_t expect_h)
{
    return ts_mix(prev_h, seq, time, data, len) == expect_h;
}
