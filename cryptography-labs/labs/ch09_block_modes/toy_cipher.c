#include "toy_cipher.h"
#include "crypto_common.h"

static uint32_t F(uint32_t r, uint32_t k)
{
    uint32_t x = r ^ k;
    x = (x << 7) | (x >> 25);
    x *= 0x9E3779B9U;
    x ^= x >> 16;
    return x;
}

static void feistel(const uint8_t in[8], uint8_t out[8], const uint8_t key[8], int decrypt)
{
    uint32_t L, R, k[8];
    uint8_t block[8];
    int i;
    memcpy(block, in, 8);
    memcpy(&L, block, 4);
    memcpy(&R, block + 4, 4);
    memcpy(k, key, 8);
    /* expand key into 8 round keys (toy) */
    {
        uint32_t k0, k1;
        memcpy(&k0, key, 4);
        memcpy(&k1, key + 4, 4);
        for (i = 0; i < 8; i++) {
            k[i] = k0 + (uint32_t)i * 0x9E3779B9U + k1;
            k0 = F(k0, k1);
            k1 = F(k1, k0 ^ 0xA5A5A5A5U);
        }
    }
    for (i = 0; i < 8; i++) {
        int round = decrypt ? (7 - i) : i;
        uint32_t nL = R;
        uint32_t nR = L ^ F(R, k[round]);
        L = nL;
        R = nR;
    }
    /* DES-style final swap: output R||L so reverse-key decrypt is a true inverse. */
    memcpy(out, &R, 4);
    memcpy(out + 4, &L, 4);
}

void toy_encrypt_block(const uint8_t in[8], uint8_t out[8], const void *key)
{
    feistel(in, out, (const uint8_t *)key, 0);
}

void toy_decrypt_block(const uint8_t in[8], uint8_t out[8], const void *key)
{
    feistel(in, out, (const uint8_t *)key, 1);
}
