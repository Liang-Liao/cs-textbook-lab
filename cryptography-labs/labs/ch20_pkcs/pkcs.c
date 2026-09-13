#include "pkcs.h"
#include "sha256.h"
#include "crypto_common.h"

int pkcs7_pad(const uint8_t *in, size_t in_len, size_t block,
              uint8_t *out, size_t out_cap, size_t *out_len)
{
    size_t pad, total, i;
    if (block == 0 || block > 255) return -1;
    pad = block - (in_len % block);
    total = in_len + pad;
    if (total > out_cap) return -1;
    memcpy(out, in, in_len);
    for (i = 0; i < pad; i++) out[in_len + i] = (uint8_t)pad;
    *out_len = total;
    return 0;
}

int pkcs7_unpad(const uint8_t *in, size_t in_len, size_t block,
                uint8_t *out, size_t out_cap, size_t *out_len)
{
    uint8_t pad;
    size_t i;
    if (block == 0 || in_len == 0 || in_len % block != 0) return -1;
    pad = in[in_len - 1];
    if (pad == 0 || pad > block) return -1;
    for (i = 0; i < pad; i++)
        if (in[in_len - 1 - i] != pad) return -1;
    if (in_len - pad > out_cap) return -1;
    memcpy(out, in, in_len - pad);
    *out_len = in_len - pad;
    return 0;
}

void hmac_sha256(const uint8_t *key, size_t key_len,
                 const uint8_t *msg, size_t msg_len,
                 uint8_t out[32])
{
    uint8_t k[64], o_key[64], i_key[64], tmp[32];
    sha256_ctx_t ctx;
    size_t i;
    memset(k, 0, 64);
    if (key_len > 64) sha256(key, key_len, k);
    else memcpy(k, key, key_len);
    for (i = 0; i < 64; i++) {
        o_key[i] = (uint8_t)(k[i] ^ 0x5c);
        i_key[i] = (uint8_t)(k[i] ^ 0x36);
    }
    sha256_init(&ctx);
    sha256_update(&ctx, i_key, 64);
    sha256_update(&ctx, msg, msg_len);
    sha256_final(&ctx, tmp);
    sha256_init(&ctx);
    sha256_update(&ctx, o_key, 64);
    sha256_update(&ctx, tmp, 32);
    sha256_final(&ctx, out);
}

int pbkdf2_sha256(const uint8_t *pass, size_t pass_len,
                  const uint8_t *salt, size_t salt_len,
                  uint32_t iterations, uint8_t *dk, size_t dk_len)
{
    uint32_t block_index = 1;
    size_t generated = 0;
    if (iterations == 0 || dk_len == 0) return -1;

    while (generated < dk_len) {
        uint8_t u[32], t[32];
        uint8_t *msg;
        size_t msg_len = salt_len + 4;
        uint32_t i, j;
        size_t take;

        msg = (uint8_t *)malloc(msg_len);
        if (!msg) return -1;
        memcpy(msg, salt, salt_len);
        msg[salt_len]     = (uint8_t)(block_index >> 24);
        msg[salt_len + 1] = (uint8_t)(block_index >> 16);
        msg[salt_len + 2] = (uint8_t)(block_index >> 8);
        msg[salt_len + 3] = (uint8_t)block_index;

        hmac_sha256(pass, pass_len, msg, msg_len, u);
        free(msg);
        memcpy(t, u, 32);
        for (i = 1; i < iterations; i++) {
            hmac_sha256(pass, pass_len, u, 32, u);
            for (j = 0; j < 32; j++) t[j] ^= u[j];
        }
        take = dk_len - generated;
        if (take > 32) take = 32;
        memcpy(dk + generated, t, take);
        generated += take;
        block_index++;
    }
    return 0;
}
