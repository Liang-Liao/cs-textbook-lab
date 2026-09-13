#ifndef KEYLEN_H
#define KEYLEN_H

#include <stdint.h>
#include <stddef.h>

/* Chapter 7: key length / brute-force cost models. */

/* Years to brute-force a `key_bits` key at `keys_per_sec`.
 * Returns -1 for invalid input; 1e300 when the real value exceeds double
 * range ("beyond astronomical"). */
double brute_force_years(int key_bits, double keys_per_sec);

/* birthday collision trials for b-bit block ≈ 2^{b/2} */
double birthday_trials(double block_bits);

/* Demo: count how many 8-bit keys match a target ct under toy E. */
int count_matching_keys(uint8_t pt[8], uint8_t ct[8], int *out_count);

#endif
