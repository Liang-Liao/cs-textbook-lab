#ifndef COMBINE_H
#define COMBINE_H

#include <stdint.h>
#include "mini_cipher.h"

/* Chapter 15: multiple encryption and meet-in-the-middle demo. */

void double_encrypt(const uint8_t in[8], uint8_t out[8],
                    uint64_t k1, uint64_t k2);
void double_decrypt(const uint8_t in[8], uint8_t out[8],
                    uint64_t k1, uint64_t k2);

void triple_encrypt_ede(const uint8_t in[8], uint8_t out[8],
                        uint64_t k1, uint64_t k2, uint64_t k3);
void triple_decrypt_ede(const uint8_t in[8], uint8_t out[8],
                        uint64_t k1, uint64_t k2, uint64_t k3);

/* MITM on double encryption with reduced keyspace (low `bits` bits). */
typedef struct {
    int bits;          /* key search width, e.g. 12 */
    int found;
    uint64_t k1, k2;
} mitm_result_t;

int mitm_double(const uint8_t pt[8], const uint8_t ct[8],
                int key_bits, mitm_result_t *res);

#endif
