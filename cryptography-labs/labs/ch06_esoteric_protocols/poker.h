#ifndef POKER_H
#define POKER_H

#include <stdint.h>
#include <stddef.h>

/* Chapter 6: mental-poker-style blind shuffle (educational).
 * Cards are 0..51. Lock is additive mod 52 (commutative). */

#define POKER_N 52

typedef struct {
    uint8_t cards[POKER_N];
} poker_deck_t;

void poker_deck_init(poker_deck_t *d);

/* lock each card: (c + key) % 52, then secret Fisher-Yates shuffle */
void poker_lock_shuffle(poker_deck_t *d, uint8_t key);

/* unlock each card: (c - key) % 52 */
void poker_unlock(poker_deck_t *d, uint8_t key);

/* 1 if multiset equals 0..51 */
int poker_is_permutation(const poker_deck_t *d);

/* full two-party protocol; out is final shuffled deck.
 * Keys drive both lock and local Fisher-Yates. Returns 0 on success. */
int poker_two_party_shuffle(poker_deck_t *out,
                            uint8_t alice_key, uint8_t bob_key);

#endif
