#ifndef TOY_CIPHER_H
#define TOY_CIPHER_H

#include <stdint.h>

/* A simple 64-bit Feistel toy cipher (not secure) used to exercise modes
 * when DES is not linked. Key is 8 bytes. */

void toy_encrypt_block(const uint8_t in[8], uint8_t out[8], const void *key);
void toy_decrypt_block(const uint8_t in[8], uint8_t out[8], const void *key);

#endif
