#ifndef NUMTHY_H
#define NUMTHY_H

#include <stdint.h>

/* Chapter 11 number-theory helpers beyond common: Euler totient, CRT. */

uint64_t euler_phi_u64(uint64_t n);

/* CRT: x ≡ a1 (mod m1), x ≡ a2 (mod m2); m1,m2 coprime. */
int crt_u64(uint64_t a1, uint64_t m1, uint64_t a2, uint64_t m2, uint64_t *x);

#endif
