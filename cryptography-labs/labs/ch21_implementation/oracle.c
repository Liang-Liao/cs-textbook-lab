#include "oracle.h"
#include "crypto_common.h"

/* Toy 64-bit block: 4-round Feistel (educational, not mix64). */

static uint32_t F(uint32_t r, uint32_t k)
{
    uint32_t x = r ^ k;
    x = (x << 7) | (x >> 25);
    x *= 0x9E3779B9U;
    x ^= x >> 16;
    return x;
}

static void enc_block(uint64_t key, const uint8_t in[8], uint8_t out[8])
{
    uint32_t L, R, k0 = (uint32_t)key, k1 = (uint32_t)(key >> 32);
    uint32_t rk[4];
    int i;
    memcpy(&L, in, 4);
    memcpy(&R, in + 4, 4);
    for (i = 0; i < 4; i++) {
        rk[i] = k0 + (uint32_t)i * 0x9E3779B9U;
        k0 = F(k0, k1);
        k1 = F(k1, k0);
    }
    for (i = 0; i < 4; i++) {
        uint32_t nL = R;
        uint32_t nR = L ^ F(R, rk[i]);
        L = nL; R = nR;
    }
    memcpy(out, &R, 4);
    memcpy(out + 4, &L, 4);
}

static void dec_block(uint64_t key, const uint8_t in[8], uint8_t out[8])
{
    uint32_t L, R, k0 = (uint32_t)key, k1 = (uint32_t)(key >> 32);
    uint32_t rk[4];
    int i;
    memcpy(&R, in, 4);
    memcpy(&L, in + 4, 4);
    for (i = 0; i < 4; i++) {
        rk[i] = k0 + (uint32_t)i * 0x9E3779B9U;
        k0 = F(k0, k1);
        k1 = F(k1, k0);
    }
    for (i = 3; i >= 0; i--) {
        uint32_t t = R;
        R = L;
        L = t ^ F(L, rk[i]);
    }
    memcpy(out, &L, 4);
    memcpy(out + 4, &R, 4);
}

static int check_pkcs7(const uint8_t *b, size_t n)
{
    uint8_t pad = b[n - 1];
    size_t i;
    if (pad == 0 || pad > n) return 0;
    for (i = 0; i < pad; i++)
        if (b[n - 1 - i] != pad) return 0;
    return 1;
}

void oracle_server_init(oracle_server_t *srv, uint64_t key)
{
    srv->key = key;
    srv->iv = 0x0123456789ABCDEFULL;
}

void oracle_encrypt(const oracle_server_t *srv,
                    const uint8_t *pt, size_t len,
                    uint8_t *out, size_t *out_len)
{
    uint8_t buf[256];
    size_t pad = 8 - (len % 8);
    size_t total = len + pad;
    size_t i, j;
    uint8_t prev[8];

    if (total > sizeof(buf)) { *out_len = 0; return; }
    memcpy(buf, pt, len);
    for (i = 0; i < pad; i++) buf[len + i] = (uint8_t)pad;

    memcpy(prev, &srv->iv, 8);
    for (i = 0; i < total; i += 8) {
        uint8_t blk[8], mid[8];
        for (j = 0; j < 8; j++) blk[j] = (uint8_t)(buf[i + j] ^ prev[j]);
        enc_block(srv->key, blk, mid);
        memcpy(out + i, mid, 8);
        memcpy(prev, mid, 8);
    }
    *out_len = total;
}

int oracle_padding_ok(const oracle_server_t *srv,
                      const uint8_t *ct, size_t ct_len)
{
    uint8_t plain[256];
    uint8_t prev[8];
    size_t i, j;
    if (ct_len == 0 || ct_len % 8 != 0 || ct_len > sizeof(plain)) return 0;
    memcpy(prev, &srv->iv, 8);
    for (i = 0; i < ct_len; i += 8) {
        uint8_t mid[8];
        dec_block(srv->key, ct + i, mid);
        for (j = 0; j < 8; j++) plain[i + j] = (uint8_t)(mid[j] ^ prev[j]);
        memcpy(prev, ct + i, 8);
    }
    return check_pkcs7(plain, ct_len);
}

/* Recover I = D_K(C_block) using only oracle queries on forged pairs. */
static int recover_intermediate(const oracle_server_t *srv,
                                const uint8_t target_ct[8],
                                uint8_t intermediate[8])
{
    uint8_t fake[16];
    size_t pos, k;

    memcpy(fake + 8, target_ct, 8);
    memset(fake, 0, 8);
    memset(intermediate, 0, 8);

    for (pos = 0; pos < 8; pos++) {
        uint8_t pad = (uint8_t)(pos + 1);
        int found = 0, guess;
        for (k = 0; k < pos; k++)
            fake[7 - k] = (uint8_t)(intermediate[7 - k] ^ pad);
        for (guess = 0; guess < 256 && !found; guess++) {
            fake[7 - pos] = (uint8_t)guess;
            if (!oracle_padding_ok(srv, fake, 16))
                continue;
            if (pos == 0) {
                /* Disambiguate pad=1 from longer accidental pads. */
                uint8_t save = fake[6];
                fake[6] ^= 0xFF;
                if (!oracle_padding_ok(srv, fake, 16)) {
                    fake[6] = save;
                    continue;
                }
                fake[6] = save;
            }
            intermediate[7 - pos] = (uint8_t)(guess ^ pad);
            found = 1;
        }
        if (!found) return -1;
    }
    return 0;
}

/* Full CBC recovery via padding oracle only (no key use). */
int padding_oracle_attack(const oracle_server_t *srv,
                          const uint8_t *ct, size_t ct_len,
                          uint8_t *pt_out, size_t *pt_len)
{
    size_t nblocks, b, j;
    uint8_t intermediate[8];
    uint8_t prev_block[8];
    uint8_t plain[256];

    if (ct_len == 0 || ct_len % 8 != 0 || ct_len > sizeof(plain)) return -1;
    nblocks = ct_len / 8;

    for (b = 0; b < nblocks; b++) {
        if (recover_intermediate(srv, ct + b * 8, intermediate) != 0)
            return -1;
        if (b == 0)
            memcpy(prev_block, &srv->iv, 8);
        else
            memcpy(prev_block, ct + (b - 1) * 8, 8);
        for (j = 0; j < 8; j++)
            plain[b * 8 + j] = (uint8_t)(intermediate[j] ^ prev_block[j]);
    }

    /* Trim PKCS#7 using recovered last-block pad. */
    {
        uint8_t pad = plain[ct_len - 1];
        size_t i;
        if (pad == 0 || pad > 8) return -1;
        for (i = 0; i < pad; i++)
            if (plain[ct_len - 1 - i] != pad) return -1;
        *pt_len = ct_len - pad;
        memcpy(pt_out, plain, *pt_len);
    }
    return 0;
}
