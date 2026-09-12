#ifndef CLRS_STRING_MATCH_H
#define CLRS_STRING_MATCH_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS Ch.32 String matching.
 * Text T[0..n-1], pattern P[0..m-1], both NUL-terminated char*.
 * Positions reported are 0-based starts (book uses 1-based s).
 */

/* CLRS 32.1 NAIVE-STRING-MATCHER. Appends offsets to matches; returns count. */
size_t naive_match(const char *T, const char *P, size_t *matches,
                   size_t max_matches);

/* CLRS 32.2 RABIN-KARP-MATCHER. q prime for modulus; d alphabet base (256). */
size_t rabin_karp_match(const char *T, const char *P, size_t *matches,
                        size_t max_matches, unsigned q, unsigned d);

/* CLRS 32.4 KMP. Uses prefix function. */
size_t kmp_match(const char *T, const char *P, size_t *matches,
                 size_t max_matches);

/* Prefix function π[0..m-1]; caller provides pi buffer of length m. */
void compute_prefix_function(const char *P, size_t m, size_t *pi);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_STRING_MATCH_H */
