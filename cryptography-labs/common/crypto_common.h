#ifndef CRYPTO_COMMON_H
#define CRYPTO_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ---------- hex / dump ---------- */
void hex_dump(const char *tag, const uint8_t *data, size_t len);
void bytes_to_hex(const uint8_t *data, size_t len, char *out /* 2*len+1 */);
int  hex_to_bytes(const char *hex, uint8_t *out, size_t max_out, size_t *out_len);

/* ---------- xor / compare ---------- */
void xor_bytes(uint8_t *dst, const uint8_t *a, const uint8_t *b, size_t len);
int  memeq_const(const uint8_t *a, const uint8_t *b, size_t len);

/* ---------- secure-ish random (platform) ---------- */
int crypto_random_bytes(uint8_t *buf, size_t len);

/* ---------- number theory (educational uint64; shared by labs) ---------- */
/* mod_mul/mod_pow are exact for any modulus (128-bit intermediate);
 * both return 0 when m is 0 or 1. */
uint64_t mod_mul_u64(uint64_t a, uint64_t b, uint64_t m);
uint64_t mod_pow_u64(uint64_t base, uint64_t exp, uint64_t m);
/* Extended Euclid with coefficients kept mod m: correct across the full
 * uint64 range; returns -1 when m==0 or a is not invertible mod m. */
int      mod_inv_u64(uint64_t a, uint64_t m, uint64_t *inv);

uint64_t gcd_u64(uint64_t a, uint64_t b);
int      ext_gcd_u64(int64_t a, int64_t b, int64_t *x, int64_t *y, int64_t *g);
int      is_prime_u64(uint64_t n, int rounds);
int      random_prime_u64(uint64_t lo, uint64_t hi, int miller_rounds, uint64_t *out);

/* Simple 64-bit avalanche mix for protocol demos (NOT a cryptographic hash). */
uint64_t mix64_u64(uint64_t x);

/* ---------- test harness ---------- */
typedef struct {
    const char *suite;
    int passed;
    int failed;
} test_stats_t;

void test_begin(test_stats_t *s, const char *suite);
void test_check(test_stats_t *s, const char *name, int cond);
void test_check_eq_hex(test_stats_t *s, const char *name,
                       const uint8_t *got, size_t got_len,
                       const uint8_t *want, size_t want_len);
int  test_end(test_stats_t *s); /* returns 0 if all passed */

#define TEST_ASSERT(s, name, cond) test_check((s), (name), (cond))

#endif /* CRYPTO_COMMON_H */
