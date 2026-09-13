#ifndef ENVELOPE_H
#define ENVELOPE_H

#include <stdint.h>
#include <stddef.h>

/* Chapter 22: password → PBKDF2 → encrypt + MAC mini envelope (educational). */

#define ENV_SALT_LEN 8
#define ENV_MAC_LEN  32
#define ENV_BLOCK    8

/* Layout of sealed blob:
 *   salt[8] || mac[32] || ciphertext[]
 * ciphertext is PKCS#7-padded, 8-byte blocks, XOR-stream from keyed mix. */

/* Max sealed size for a given plaintext length. */
size_t env_sealed_size(size_t pt_len);

/* Seal: pass → PBKDF2(64B = 32 enc || 32 mac), pad, encrypt, MAC(salt||ciphertext).
 * Salt is always generated internally (BCrypt, with lab fallback). Returns 0 on success. */
int env_seal(const uint8_t *pass, size_t pass_len,
             const uint8_t *pt, size_t pt_len,
             uint8_t *out, size_t out_cap, size_t *out_len);

/* Open: re-derive, verify MAC (constant-time), decrypt, unpad.
 * Returns 0 on success; non-zero on bad MAC / bad pad / bad pass. */
int env_open(const uint8_t *pass, size_t pass_len,
             const uint8_t *sealed, size_t sealed_len,
             uint8_t *pt_out, size_t pt_cap, size_t *pt_len);

#endif
