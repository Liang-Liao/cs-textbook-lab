#ifndef RNGTEST_H
#define RNGTEST_H

#include <stdint.h>
#include <stddef.h>

/* Chapter 17: basic randomness tests. */

/* Frequency test: returns |count1 - n/2| / (n/2) in [0,1] roughly. */
double rng_monobit_bias(const uint8_t *buf, size_t len);

/* Runs test (FIPS 140-2 style) over the first 20000 bits of input: the 0-run
 * and 1-run counts of each length 1..5, plus the bucket of runs >= 6, must
 * stay inside the official interval. Needs at least 2500 bytes; returns 0 on
 * shorter input or when any bound is violated. */
int rng_runs_ok(const uint8_t *buf, size_t len);

/* Serial: byte histogram max deviation from uniform. */
double rng_byte_max_dev(const uint8_t *buf, size_t len);

#endif
