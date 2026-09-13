#include "mini_cipher.h"
#include "crypto_common.h"

static uint32_t F(uint32_t r, uint32_t k)
{
    uint32_t x = r ^ k;
    x = (x << 7) | (x >> 25);
    x *= 0x9E3779B9U;
    x ^= x >> 16;
    return x;
}

static void feistel(const uint8_t in[8], uint8_t out[8], uint64_t key, int decrypt)
{
    uint32_t L, R, k[8];
    uint32_t k0 = (uint32_t)key, k1 = (uint32_t)(key >> 32);
    int i;
    memcpy(&L, in, 4);
    memcpy(&R, in + 4, 4);
    for (i = 0; i < 8; i++) {
        k[i] = k0 + (uint32_t)i * 0x9E3779B9U + k1;
        k0 = F(k0, k1);
        k1 = F(k1, k0 ^ 0xA5A5A5A5U);
    }
    for (i = 0; i < 8; i++) {
        int round = decrypt ? (7 - i) : i;
        uint32_t nL = R;
        uint32_t nR = L ^ F(R, k[round]);
        L = nL;
        R = nR;
    }
    memcpy(out, &R, 4);
    memcpy(out + 4, &L, 4);
}

void mini_encrypt_block(const uint8_t in[8], uint8_t out[8], const void *key)
{
    feistel(in, out, *(const uint64_t *)key, 0);
}

void mini_decrypt_block(const uint8_t in[8], uint8_t out[8], const void *key)
{
    feistel(in, out, *(const uint64_t *)key, 1);
}
