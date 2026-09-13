#ifndef GF2N_H
#define GF2N_H

#include <stdint.h>

/* Chapter 11: GF(2^n) helpers — AES-style xtime and carry-less multiply
 * reduced by an irreducible polynomial (default AES: x^8+x^4+x^3+x+1). */

uint8_t  gf2_8_xtime(uint8_t a);
uint8_t  gf2_8_mul(uint8_t a, uint8_t b); /* AES field */
uint8_t  gf2_8_inv(uint8_t a);            /* a^{254} via square-multiply */

uint32_t gf2_32_clmul(uint32_t a, uint32_t b); /* no reduction, for study */
uint32_t gf2_32_mod(uint32_t r, uint32_t mod); /* reduce by mod poly */

#endif
