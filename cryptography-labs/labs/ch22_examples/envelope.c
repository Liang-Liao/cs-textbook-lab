#include "envelope.h"
#include "sha256.h"
#include "crypto_common.h"

/* ---- local PKCS#7 (lab-local copy; no cross-lab include) ---- */
static int env_pad(const uint8_t *in, size_t in_len, size_t block,
                   uint8_t *out, size_t out_cap, size_t *out_len)
{
    size_t pad = block - (in_len % block);
    size_t total = in_len + pad;
    size_t i;
    if (block == 0 || block > 255 || total > out_cap) return -1;
    memcpy(out, in, in_len);
    for (i = 0; i < pad; i++) out[in_len + i] = (uint8_t)pad;
    *out_len = total;
    return 0;
}

static int env_unpad(const uint8_t *in, size_t in_len, size_t block,
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

/* ---- HMAC-SHA256 ---- */
static void env_hmac(const uint8_t *key, size_t key_len,
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

/* ---- PBKDF2-HMAC-SHA256 (minimal, lab-local) ---- */
static int env_pbkdf2(const uint8_t *pass, size_t pass_len,
                      const uint8_t *salt, size_t salt_len,
                      uint32_t iterations, uint8_t *dk, size_t dk_len)
{
    uint32_t block_index = 1;
    size_t generated = 0;
    if (iterations == 0 || dk_len == 0) return -1;

    while (generated < dk_len) {
        uint8_t u[32], t[32];
        uint8_t msg[64];
        size_t msg_len = salt_len + 4;
        uint32_t i, j;
        size_t take;

        if (msg_len > sizeof(msg)) return -1;
        memcpy(msg, salt, salt_len);
        msg[salt_len]     = (uint8_t)(block_index >> 24);
        msg[salt_len + 1] = (uint8_t)(block_index >> 16);
        msg[salt_len + 2] = (uint8_t)(block_index >> 8);
        msg[salt_len + 3] = (uint8_t)block_index;

        env_hmac(pass, pass_len, msg, msg_len, u);
        memcpy(t, u, 32);
        for (i = 1; i < iterations; i++) {
            env_hmac(pass, pass_len, u, 32, u);
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

/* ---- 8-byte block "cipher": keyed XOR keystream from mix64 (demo only) ---- */
static uint64_t env_ks_block(uint64_t key, uint64_t counter)
{
    return mix64_u64(key ^ mix64_u64(counter));
}

static void env_xor_stream(uint64_t key, uint8_t *buf, size_t len)
{
    size_t off = 0;
    uint64_t ctr = 0;
    while (off < len) {
        uint64_t ks = env_ks_block(key, ctr++);
        size_t n = len - off;
        size_t i;
        if (n > 8) n = 8;
        for (i = 0; i < n; i++)
            buf[off + i] ^= (uint8_t)(ks >> (8 * i));
        off += n;
    }
}

size_t env_sealed_size(size_t pt_len)
{
    size_t padded = pt_len + (ENV_BLOCK - (pt_len % ENV_BLOCK));
    return ENV_SALT_LEN + ENV_MAC_LEN + padded;
}

static int env_hmac_payload(const uint8_t mac_key[32],
                             const uint8_t *salt, const uint8_t *ct, size_t ct_len,
                             uint8_t mac[32])
{
    uint8_t *msg = (uint8_t *)malloc(ENV_SALT_LEN + ct_len);
    if (!msg) return -1;
    memcpy(msg, salt, ENV_SALT_LEN);
    memcpy(msg + ENV_SALT_LEN, ct, ct_len);
    env_hmac(mac_key, 32, msg, ENV_SALT_LEN + ct_len, mac);
    free(msg);
    return 0;
}

int env_seal(const uint8_t *pass, size_t pass_len,
             const uint8_t *pt, size_t pt_len,
             uint8_t *out, size_t out_cap, size_t *out_len)
{
    uint8_t salt[ENV_SALT_LEN];
    uint8_t dk[64];
    uint8_t mac[ENV_MAC_LEN];
    size_t ct_len = 0;
    size_t need;

    if (!pass || !pt || !out || !out_len) return -1;
    need = env_sealed_size(pt_len);
    if (out_cap < need) return -1;

    if (crypto_random_bytes(salt, ENV_SALT_LEN) != 0)
        return -1;

    if (env_pbkdf2(pass, pass_len, salt, ENV_SALT_LEN, 1000, dk, 64) != 0)
        return -1;

    /* encrypt: PKCS#7 then XOR stream with first 8 bytes of enc-key as u64 LE */
    if (env_pad(pt, pt_len, ENV_BLOCK, out + ENV_SALT_LEN + ENV_MAC_LEN,
                out_cap - ENV_SALT_LEN - ENV_MAC_LEN, &ct_len) != 0)
        return -1;

    {
        uint64_t ekey = 0;
        int i;
        for (i = 0; i < 8; i++)
            ekey |= (uint64_t)dk[i] << (8 * i);
        env_xor_stream(ekey, out + ENV_SALT_LEN + ENV_MAC_LEN, ct_len);
    }

    if (env_hmac_payload(dk + 32, salt, out + ENV_SALT_LEN + ENV_MAC_LEN, ct_len, mac) != 0)
        return -1;
    memcpy(out, salt, ENV_SALT_LEN);
    memcpy(out + ENV_SALT_LEN, mac, ENV_MAC_LEN);
    *out_len = ENV_SALT_LEN + ENV_MAC_LEN + ct_len;
    return 0;
}

int env_open(const uint8_t *pass, size_t pass_len,
             const uint8_t *sealed, size_t sealed_len,
             uint8_t *pt_out, size_t pt_cap, size_t *pt_len)
{
    const uint8_t *salt, *mac, *ct;
    size_t ct_len;
    uint8_t dk[64];
    uint8_t expect[ENV_MAC_LEN];
    uint64_t ekey = 0;
    int i;

    if (!pass || !sealed || !pt_out || !pt_len) return -1;
    if (sealed_len < ENV_SALT_LEN + ENV_MAC_LEN + ENV_BLOCK) return -1;
    salt = sealed;
    mac  = sealed + ENV_SALT_LEN;
    ct   = sealed + ENV_SALT_LEN + ENV_MAC_LEN;
    ct_len = sealed_len - ENV_SALT_LEN - ENV_MAC_LEN;
    if (ct_len % ENV_BLOCK != 0) return -1;

    if (env_pbkdf2(pass, pass_len, salt, ENV_SALT_LEN, 1000, dk, 64) != 0)
        return -1;

    if (env_hmac_payload(dk + 32, salt, ct, ct_len, expect) != 0)
        return -1;
    if (!memeq_const(mac, expect, ENV_MAC_LEN))
        return -1;

    {
        uint8_t *tmp = (uint8_t *)malloc(ct_len);
        if (!tmp) return -1;
        memcpy(tmp, ct, ct_len);
        for (i = 0; i < 8; i++)
            ekey |= (uint64_t)dk[i] << (8 * i);
        env_xor_stream(ekey, tmp, ct_len);
        i = env_unpad(tmp, ct_len, ENV_BLOCK, pt_out, pt_cap, pt_len);
        free(tmp);
        return i;
    }
}
