#ifndef RSA_H
#define RSA_H

#include <stdint.h>
#include <stddef.h>

/* Chapter 19: Educational RSA with small primes (toy size).
 * Uses uint64 modular arithmetic — keys stay under ~32-bit modulus
 * so messages must fit in < n. Fine for protocol demos, NOT production. */

typedef struct {
    uint64_t n;   /* modulus */
    uint64_t e;   /* public exponent */
    uint64_t d;   /* private exponent */
} rsa_keypair_t;

/* Generate keypair with primes in [lo,hi]. e usually 65537 or 3/17. */
int rsa_generate(rsa_keypair_t *kp, uint64_t p_lo, uint64_t p_hi, uint64_t e);

/* Encrypt/decrypt a single integer message m < n */
uint64_t rsa_public(uint64_t m, const rsa_keypair_t *kp);
uint64_t rsa_private(uint64_t c, const rsa_keypair_t *kp);

/* PKCS#1 v1.5-like educational padding: 02||PS||00||M (no leading 0x00 byte). */
int rsa_public_bytes(const uint8_t *msg, size_t msg_len,
                     uint8_t *out, size_t *out_len,
                     const rsa_keypair_t *kp);
int rsa_private_bytes(const uint8_t *in, size_t in_len,
                      uint8_t *out, size_t *out_len,
                      const rsa_keypair_t *kp);

#endif
