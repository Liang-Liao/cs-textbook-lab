#ifndef TICKET_H
#define TICKET_H

#include <stdint.h>
#include <stddef.h>

/* Chapter 4: simplified ticket / timestamp-chain structures (educational).
 * Kerberos-like encrypted ticket fields and a hash-chain timestamp log. */

#define TICKET_ID_LEN 8

typedef struct {
    uint8_t  client_id[TICKET_ID_LEN];
    uint8_t  server_id[TICKET_ID_LEN];
    uint64_t session_key;   /* K_c,s  (toy 64-bit) */
    uint64_t timestamp;     /* not_before style */
    uint64_t lifetime;
} ticket_plain_t;

typedef struct {
    uint8_t enc[48];        /* >= sizeof(ticket_plain_t), XOR-stream ciphertext */
} ticket_t;

/* Encrypt ticket under server key K using mix64 keystream (NOT secure). */
void ticket_issue(const ticket_plain_t *pt, uint64_t server_key, ticket_t *out);
int  ticket_open(const ticket_t *t, uint64_t server_key, ticket_plain_t *out);

/* Append-only timestamp chain: h_{i+1} = mix(h_i || time || data_hash) */
typedef struct {
    uint64_t h;
    uint64_t seq;
} ts_chain_t;

void ts_chain_init(ts_chain_t *c);
void ts_chain_append(ts_chain_t *c, uint64_t time, const uint8_t *data, size_t len);
int  ts_chain_check_link(uint64_t prev_h, uint64_t seq,
                         uint64_t time, const uint8_t *data, size_t len,
                         uint64_t expect_h);

#endif
