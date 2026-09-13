#include "poker.h"
#include "crypto_common.h"

void poker_deck_init(poker_deck_t *d)
{
    int i;
    for (i = 0; i < POKER_N; i++)
        d->cards[i] = (uint8_t)i;
}

/* Deterministic PRNG for reproducible tests (not cryptographic). */
static uint32_t prng_next(uint32_t *s)
{
    *s = (*s * 1664525u) + 1013904223u;
    return *s;
}

static void fisher_yates(poker_deck_t *d, uint32_t *rng)
{
    int i;
    for (i = POKER_N - 1; i > 0; i--) {
        int j = (int)(prng_next(rng) % (uint32_t)(i + 1));
        uint8_t t = d->cards[i];
        d->cards[i] = d->cards[j];
        d->cards[j] = t;
    }
}

void poker_lock_shuffle(poker_deck_t *d, uint8_t key)
{
    int i;
    uint32_t rng = key;
    for (i = 0; i < POKER_N; i++)
        d->cards[i] = (uint8_t)((d->cards[i] + key) % POKER_N);
    fisher_yates(d, &rng);
}

void poker_unlock(poker_deck_t *d, uint8_t key)
{
    int i;
    for (i = 0; i < POKER_N; i++) {
        int v = (int)d->cards[i] - (int)key;
        while (v < 0) v += POKER_N;
        d->cards[i] = (uint8_t)(v % POKER_N);
    }
}

int poker_is_permutation(const poker_deck_t *d)
{
    int seen[POKER_N];
    int i;
    memset(seen, 0, sizeof(seen));
    for (i = 0; i < POKER_N; i++) {
        if (d->cards[i] >= POKER_N) return 0;
        if (seen[d->cards[i]]) return 0;
        seen[d->cards[i]] = 1;
    }
    return 1;
}

int poker_two_party_shuffle(poker_deck_t *out,
                            uint8_t alice_key, uint8_t bob_key)
{
    poker_deck_t a, mid;

    poker_deck_init(&a);
    /* Alice locks+shuffles */
    poker_lock_shuffle(&a, alice_key);
    /* Bob locks+shuffles Alice's output */
    mid = a;
    poker_lock_shuffle(&mid, bob_key);
    /* Alice unlocks her layer (positions remain Bob's shuffle of Alice's) */
    poker_unlock(&mid, alice_key);
    /* Bob unlocks his layer */
    *out = mid;
    poker_unlock(out, bob_key);
    return poker_is_permutation(out) ? 0 : -1;
}
