#ifndef ADVPROTO_H
#define ADVPROTO_H

#include <stdint.h>
#include <stddef.h>

/* Chapter 5: simplified commitment and "Schnorr-like" ZK demo. */

/* Commitment: c = H(rlen || r || mlen || m); open later. */
void commit(const uint8_t *r, size_t rlen,
            const uint8_t *m, size_t mlen,
            uint8_t c[32]);
int  open_check(const uint8_t *r, size_t rlen,
                const uint8_t *m, size_t mlen,
                const uint8_t c[32]);

/* Toy discrete-log ZK (prove knowledge of x s.t. X=g^x mod p) — Fiat-Shamir style. */
typedef struct {
    uint64_t p, g, X;
} zk_pub_t;

typedef struct {
    uint64_t A, z, c;
} zk_proof_t;

/* Returns 0 on success, -1 when the system RNG fails (no fixed fallback
 * nonce, which would make proofs replayable). */
int  zk_prove(const zk_pub_t *pub, uint64_t x, zk_proof_t *proof);
int  zk_verify(const zk_pub_t *pub, const zk_proof_t *proof);

#endif
