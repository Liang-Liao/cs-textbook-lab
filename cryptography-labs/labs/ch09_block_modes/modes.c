#include "modes.h"
#include "crypto_common.h"

int mode_ecb_encrypt(const uint8_t *in, uint8_t *out, size_t len,
                     block_encrypt_fn enc, const void *key)
{
    size_t i;
    if (len % 8 != 0 || enc == NULL) return -1;
    for (i = 0; i < len; i += 8)
        enc(in + i, out + i, key);
    return 0;
}

int mode_ecb_decrypt(const uint8_t *in, uint8_t *out, size_t len,
                     block_decrypt_fn dec, const void *key)
{
    size_t i;
    if (len % 8 != 0 || dec == NULL) return -1;
    for (i = 0; i < len; i += 8)
        dec(in + i, out + i, key);
    return 0;
}

int mode_cbc_encrypt(const uint8_t *in, uint8_t *out, size_t len,
                     const uint8_t iv[8],
                     block_encrypt_fn enc, const void *key)
{
    uint8_t prev[8];
    size_t i, j;
    if (len % 8 != 0 || enc == NULL || iv == NULL) return -1;
    memcpy(prev, iv, 8);
    for (i = 0; i < len; i += 8) {
        uint8_t blk[8];
        for (j = 0; j < 8; j++) blk[j] = (uint8_t)(in[i + j] ^ prev[j]);
        enc(blk, out + i, key);
        memcpy(prev, out + i, 8);
    }
    return 0;
}

int mode_cbc_decrypt(const uint8_t *in, uint8_t *out, size_t len,
                     const uint8_t iv[8],
                     block_decrypt_fn dec, const void *key)
{
    uint8_t prev[8], cur[8];
    size_t i, j;
    if (len % 8 != 0 || dec == NULL || iv == NULL) return -1;
    memcpy(prev, iv, 8);
    for (i = 0; i < len; i += 8) {
        memcpy(cur, in + i, 8);
        dec(in + i, out + i, key);
        for (j = 0; j < 8; j++) out[i + j] ^= prev[j];
        memcpy(prev, cur, 8);
    }
    return 0;
}

void mode_cfb8_encrypt(const uint8_t *in, uint8_t *out, size_t len,
                       const uint8_t iv[8],
                       block_encrypt_fn enc, const void *key)
{
    uint8_t shift[8], keystream[8];
    size_t i;
    memcpy(shift, iv, 8);
    for (i = 0; i < len; i++) {
        enc(shift, keystream, key);
        out[i] = (uint8_t)(in[i] ^ keystream[0]);
        memmove(shift, shift + 1, 7);
        shift[7] = out[i];
    }
}

void mode_cfb8_decrypt(const uint8_t *in, uint8_t *out, size_t len,
                       const uint8_t iv[8],
                       block_encrypt_fn enc, const void *key)
{
    uint8_t shift[8], keystream[8], c;
    size_t i;
    memcpy(shift, iv, 8);
    for (i = 0; i < len; i++) {
        /* keep the ciphertext byte: in may alias out (in-place decrypt) */
        c = in[i];
        enc(shift, keystream, key);
        out[i] = (uint8_t)(c ^ keystream[0]);
        memmove(shift, shift + 1, 7);
        shift[7] = c;
    }
}

void mode_ofb(const uint8_t *in, uint8_t *out, size_t len,
              const uint8_t iv[8],
              block_encrypt_fn enc, const void *key)
{
    uint8_t state[8], keystream[8];
    size_t i;
    memcpy(state, iv, 8);
    for (i = 0; i < len; ) {
        size_t chunk = (len - i) < 8 ? (len - i) : 8;
        size_t j;
        enc(state, keystream, key);
        memcpy(state, keystream, 8);
        for (j = 0; j < chunk; j++)
            out[i + j] = (uint8_t)(in[i + j] ^ keystream[j]);
        i += chunk;
    }
}

void mode_ctr(const uint8_t *in, uint8_t *out, size_t len,
              const uint8_t nonce_ctr[8],
              block_encrypt_fn enc, const void *key)
{
    uint8_t ctr[8], keystream[8];
    size_t i, j;
    memcpy(ctr, nonce_ctr, 8);
    for (i = 0; i < len; ) {
        size_t chunk = (len - i) < 8 ? (len - i) : 8;
        enc(ctr, keystream, key);
        for (j = 0; j < chunk; j++)
            out[i + j] = (uint8_t)(in[i + j] ^ keystream[j]);
        /* increment big-endian 64-bit counter */
        for (j = 8; j-- > 0; ) {
            if (++ctr[j] != 0) break;
        }
        i += chunk;
    }
}
