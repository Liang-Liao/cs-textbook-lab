#ifndef MINI_CIPHER_H
#define MINI_CIPHER_H

#include <stdint.h>

/* Local 64-bit toy Feistel so this lab stays independent of DES/blowfish labs. */
void mini_encrypt_block(const uint8_t in[8], uint8_t out[8], const void *key /* uint64_t* */);
void mini_decrypt_block(const uint8_t in[8], uint8_t out[8], const void *key);

#endif
