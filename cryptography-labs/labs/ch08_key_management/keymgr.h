#ifndef KEYMGR_H
#define KEYMGR_H

#include <stdint.h>
#include <stddef.h>

/* Chapter 8: key hierarchy and certificate-ish fields (toy). */

#define KM_MAX_KEYS 8
#define KM_ID_LEN   8

typedef enum { KM_KEK = 1, KM_DEK = 2 } km_role_t;

typedef struct {
    uint8_t  id[KM_ID_LEN];
    km_role_t role;
    uint64_t material;
    uint64_t not_after;
    int      revoked;
} km_key_t;

typedef struct {
    km_key_t keys[KM_MAX_KEYS];
    int n;
} km_store_t;

void km_init(km_store_t *st);
int  km_add_key(km_store_t *st, const char *id, km_role_t role,
                uint64_t material, uint64_t not_after);
const km_key_t *km_find(const km_store_t *st, const char *id);
int  km_revoke(km_store_t *st, const char *id, uint64_t now);
int  km_usable(const km_key_t *k, uint64_t now);

/* Wrap DEK under KEK (XOR mix toy). */
int km_wrap(const km_store_t *st, const char *kek_id, const char *dek_id,
            uint8_t wrapped[8], uint64_t now);
int km_unwrap(const km_store_t *st, const char *kek_id,
              const uint8_t wrapped[8], uint64_t *dek_out, uint64_t now);

#endif
