#ifndef DH_H
#define DH_H

#include <stdint.h>

/* Chapter 3 / 19: Diffie-Hellman key exchange (educational, uint64). */

typedef struct {
    uint64_t p; /* prime modulus */
    uint64_t g; /* generator */
} dh_params_t;

typedef struct {
    uint64_t priv;
    uint64_t pub;
} dh_party_t;

void dh_default_params(dh_params_t *p); /* small well-known params for demos */
int  dh_derive_public(dh_party_t *party, const dh_params_t *params);
uint64_t dh_shared_secret(uint64_t peer_pub, uint64_t own_priv, const dh_params_t *params);

#endif
