#ifndef CONTRACT_H
#define CONTRACT_H

#include <stdint.h>
#include <stddef.h>

/* Chapter 6: simultaneous contract signing (toy).
 * Note: unlike the book's per-bit commitments, this demo commits to the whole
 * contract text with one hash — the binding/opening flow is the same. */

#define CONTRACT_DIGEST 32

typedef struct {
    uint8_t digest[CONTRACT_DIGEST];
    int committed;
} contract_commit_t;

/* c = SHA256(plen || party || tlen || text || nlen || nonce), lengths 8-byte LE */
void contract_commit(const char *party,
                     const uint8_t *text, size_t text_len,
                     const uint8_t *nonce, size_t nonce_len,
                     contract_commit_t *out);

/* Verify opening against a prior commitment. Returns 1 if match. */
int contract_open_check(const char *party,
                        const uint8_t *text, size_t text_len,
                        const uint8_t *nonce, size_t nonce_len,
                        const contract_commit_t *c);

/* Atomic-ish exchange demo helpers.
 * Both sides commit, then both open. If one opens and the other does not,
 * the honest party can still prove the cheater was committed. */
typedef struct {
    contract_commit_t a_commit;
    contract_commit_t b_commit;
    int a_opened;
    int b_opened;
    uint8_t a_text[256];
    size_t a_text_len;
    uint8_t a_nonce[32];
    size_t a_nonce_len;
    uint8_t b_text[256];
    size_t b_text_len;
    uint8_t b_nonce[32];
    size_t b_nonce_len;
} contract_session_t;

void contract_session_init(contract_session_t *s);
int  contract_exchange_commit(contract_session_t *s,
                              const uint8_t *a_text, size_t a_text_len,
                              const uint8_t *a_nonce, size_t a_nonce_len,
                              const uint8_t *b_text, size_t b_text_len,
                              const uint8_t *b_nonce, size_t b_nonce_len);
/* open_party: 'A' or 'B'. Returns 0 on success. */
int  contract_exchange_open(contract_session_t *s, char party);
/* 1 if both parties opened and both openings verify. */
int  contract_exchange_complete(const contract_session_t *s);
/* After A opens, if B refuses: prove B is still bound by commitment. */
int  contract_prove_bound(const contract_session_t *s, char party,
                          const uint8_t *text, size_t text_len,
                          const uint8_t *nonce, size_t nonce_len);

#endif
