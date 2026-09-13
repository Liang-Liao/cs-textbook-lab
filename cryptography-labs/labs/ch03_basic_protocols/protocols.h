#ifndef PROTOCOLS_H
#define PROTOCOLS_H

#include <stdint.h>
#include <stddef.h>

/* Chapter 3: basic protocols (educational demos of protocol structure). */

/* Simple shared-key challenge-response using a one-way function.
 * Prover and verifier share secret S. */
typedef struct {
    uint64_t secret;
} auth_shared_t;

/* Verifier -> Prover: nonce R. Returns 0 and writes *out, or -1 if the
 * system RNG fails (no silent fallback challenge). */
int auth_challenge(uint64_t *out);
/* Prover: A = H(S || R)  (using toy mix) */
uint64_t auth_response(uint64_t secret, uint64_t challenge);
/* Verifier checks */
int auth_verify(const auth_shared_t *v, uint64_t challenge, uint64_t response);

#endif
