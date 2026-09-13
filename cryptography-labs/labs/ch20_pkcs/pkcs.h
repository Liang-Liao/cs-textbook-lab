#ifndef PKCS_H
#define PKCS_H

#include <stdint.h>
#include <stddef.h>

/* Chapter 20: PKCS structures — padding and password-based KDF (educational). */

/* PKCS#7 / PKCS#5 padding for block size 1..255. */
int pkcs7_pad(const uint8_t *in, size_t in_len, size_t block,
              uint8_t *out, size_t out_cap, size_t *out_len);
int pkcs7_unpad(const uint8_t *in, size_t in_len, size_t block,
                uint8_t *out, size_t out_cap, size_t *out_len);

/* HMAC-SHA256 (RFC 2104) — also usable directly. */
void hmac_sha256(const uint8_t *key, size_t key_len,
                 const uint8_t *msg, size_t msg_len,
                 uint8_t out[32]);

/* PBKDF2-HMAC-SHA256 (subset for lab; uses SHA-256 from ch18 copied locally). */
int pbkdf2_sha256(const uint8_t *pass, size_t pass_len,
                  const uint8_t *salt, size_t salt_len,
                  uint32_t iterations, uint8_t *dk, size_t dk_len);

#endif
