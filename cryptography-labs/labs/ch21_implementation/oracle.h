#ifndef ORACLE_H
#define ORACLE_H

#include <stdint.h>
#include <stddef.h>

/* Chapter 21: padding oracle attack demo (educational).
 * Local 4-round Feistel block cipher (not a real cipher). */

typedef struct {
    uint64_t key;
    uint64_t iv; /* CBC initialisation vector (chains into the first block) */
} oracle_server_t;

void oracle_server_init(oracle_server_t *srv, uint64_t key);

/* Encrypt under CBC with 8-byte local Feistel blocks (demo only). */
void oracle_encrypt(const oracle_server_t *srv,
                    const uint8_t *pt, size_t len,
                    uint8_t *out, size_t *out_len);

/* Returns 1 if padding valid after CBC decrypt, 0 otherwise. */
int oracle_padding_ok(const oracle_server_t *srv,
                      const uint8_t *ct, size_t ct_len);

/* Vaudenay padding-oracle attack: recover full plaintext using only
 * oracle_padding_ok (no key). Trims PKCS#7. */
int padding_oracle_attack(const oracle_server_t *srv,
                          const uint8_t *ct, size_t ct_len,
                          uint8_t *pt_out, size_t *pt_len);

#endif
