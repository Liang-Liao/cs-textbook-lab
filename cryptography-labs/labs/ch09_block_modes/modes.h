#ifndef MODES_H
#define MODES_H

#include <stdint.h>
#include <stddef.h>

/* Chapter 9: block cipher modes of operation.
 * Block size 8 bytes (matches DES / toy cipher used here). */

typedef void (*block_encrypt_fn)(const uint8_t in[8], uint8_t out[8], const void *key);
typedef void (*block_decrypt_fn)(const uint8_t in[8], uint8_t out[8], const void *key);

/* All length arguments are in bytes. ECB/CBC (block-oriented) return 0 on
 * success and -1 when len % 8 != 0 or the cipher/IV pointer is NULL.
 * CFB-8/OFB/CTR (stream-oriented) accept any len. All modes support
 * in == out (in-place operation). */

int mode_ecb_encrypt(const uint8_t *in, uint8_t *out, size_t len,
                     block_encrypt_fn enc, const void *key);
int mode_ecb_decrypt(const uint8_t *in, uint8_t *out, size_t len,
                     block_decrypt_fn dec, const void *key);

int mode_cbc_encrypt(const uint8_t *in, uint8_t *out, size_t len,
                     const uint8_t iv[8],
                     block_encrypt_fn enc, const void *key);
int mode_cbc_decrypt(const uint8_t *in, uint8_t *out, size_t len,
                     const uint8_t iv[8],
                     block_decrypt_fn dec, const void *key);

/* CFB-8 (byte-oriented CFB) — encryption and decryption both take the
 * block cipher's *encrypt* direction */
void mode_cfb8_encrypt(const uint8_t *in, uint8_t *out, size_t len,
                       const uint8_t iv[8],
                       block_encrypt_fn enc, const void *key);
void mode_cfb8_decrypt(const uint8_t *in, uint8_t *out, size_t len,
                       const uint8_t iv[8],
                       block_encrypt_fn enc, const void *key);

void mode_ofb(const uint8_t *in, uint8_t *out, size_t len,
              const uint8_t iv[8],
              block_encrypt_fn enc, const void *key);

/* CTR with 64-bit big-endian counter in iv (block-aligned stream) */
void mode_ctr(const uint8_t *in, uint8_t *out, size_t len,
              const uint8_t nonce_ctr[8],
              block_encrypt_fn enc, const void *key);

#endif
