#include "crypto_common.h"
#include "poker.h"
#include "contract.h"
#include "sha256.h"

int main(void)
{
    test_stats_t s;
    test_begin(&s, "Chapter 6: Esoteric Protocols (toy)");

    /* Local SHA-256 KAT (FIPS 180) */
    {
        uint8_t d[32];
        char hex[65];
        sha256((const uint8_t *)"abc", 3, d);
        bytes_to_hex(d, 32, hex);
        test_check(&s, "sha256(abc) vector",
                   strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0);
    }

    /* Mental poker: two-party lock/shuffle preserves the multiset. */
    {
        poker_deck_t final;
        test_check(&s, "two-party shuffle ok",
                   poker_two_party_shuffle(&final, 17, 42) == 0);
        test_check(&s, "final deck is permutation", poker_is_permutation(&final) == 1);
        /* Commutativity of lock: E_a(E_b(c)) == E_b(E_a(c)) for single card. */
        {
            uint8_t c = 7, a = 11, b = 29;
            uint8_t ab = (uint8_t)((c + a) % POKER_N);
            ab = (uint8_t)((ab + b) % POKER_N);
            uint8_t ba = (uint8_t)((c + b) % POKER_N);
            ba = (uint8_t)((ba + a) % POKER_N);
            test_check(&s, "lock commutes", ab == ba);
        }
    }

    /* Simultaneous contract signing: both commit then both open. */
    {
        contract_session_t sess;
        uint8_t ta[] = "Alice pays 10";
        uint8_t tb[] = "Bob ships item";
        uint8_t na[8] = {1,2,3,4,5,6,7,8};
        uint8_t nb[8] = {8,7,6,5,4,3,2,1};
        contract_session_init(&sess);
        test_check(&s, "exchange commit",
                   contract_exchange_commit(&sess, ta, sizeof(ta)-1, na, 8,
                                            tb, sizeof(tb)-1, nb, 8) == 0);
        test_check(&s, "A opens", contract_exchange_open(&sess, 'A') == 0);
        test_check(&s, "B opens", contract_exchange_open(&sess, 'B') == 0);
        test_check(&s, "exchange complete", contract_exchange_complete(&sess) == 1);
    }

    /* Bob commits, Alice opens; Bob refuses — Alice can still prove Bob bound. */
    {
        contract_session_t sess;
        uint8_t ta[] = "Alice signed";
        uint8_t tb[] = "Bob signed";
        uint8_t na[4] = {9,9,9,9};
        uint8_t nb[4] = {1,1,1,1};
        uint8_t wrong[] = "not what Bob signed";
        contract_session_init(&sess);
        contract_exchange_commit(&sess, ta, sizeof(ta)-1, na, 4,
                                 tb, sizeof(tb)-1, nb, 4);
        test_check(&s, "A opens first", contract_exchange_open(&sess, 'A') == 0);
        test_check(&s, "session not complete without B",
                   contract_exchange_complete(&sess) == 0);
        test_check(&s, "prove Bob bound to real text",
                   contract_prove_bound(&sess, 'B', tb, sizeof(tb)-1, nb, 4) == 1);
        test_check(&s, "reject forged Bob text",
                   contract_prove_bound(&sess, 'B', wrong, sizeof(wrong)-1, nb, 4) == 0);
    }

    /* Binding: wrong nonce fails open. */
    {
        contract_commit_t c;
        uint8_t m[] = "contract";
        uint8_t n1[4] = {1,2,3,4};
        uint8_t n2[4] = {4,3,2,1};
        contract_commit("Alice", m, sizeof(m)-1, n1, 4, &c);
        test_check(&s, "open with correct nonce",
                   contract_open_check("Alice", m, sizeof(m)-1, n1, 4, &c) == 1);
        test_check(&s, "reject wrong nonce",
                   contract_open_check("Alice", m, sizeof(m)-1, n2, 4, &c) == 0);
    }

    /* Binding: length framing — two (text, nonce) splits with the identical
     * concatenation ("pay $10"+"1" vs "pay $1"+"01") must not share a digest. */
    {
        contract_commit_t c1, c2;
        contract_commit("Alice", (const uint8_t *)"pay $10", 7, (const uint8_t *)"1", 1, &c1);
        contract_commit("Alice", (const uint8_t *)"pay $1", 6, (const uint8_t *)"01", 2, &c2);
        test_check(&s, "commit binds field boundaries",
                   memcmp(c1.digest, c2.digest, CONTRACT_DIGEST) != 0);
        test_check(&s, "reject cross-split open",
                   contract_open_check("Alice", (const uint8_t *)"pay $1", 6,
                                       (const uint8_t *)"01", 2, &c1) == 0 &&
                   contract_open_check("Alice", (const uint8_t *)"pay $10", 7,
                                       (const uint8_t *)"1", 1, &c2) == 0);
    }

    return test_end(&s);
}
