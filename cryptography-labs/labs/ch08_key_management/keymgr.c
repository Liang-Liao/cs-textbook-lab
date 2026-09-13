#include "keymgr.h"
#include "crypto_common.h"

void km_init(km_store_t *st)
{
    st->n = 0;
}

static void set_id(uint8_t dst[KM_ID_LEN], const char *id)
{
    size_t i;
    memset(dst, 0, KM_ID_LEN);
    for (i = 0; i < KM_ID_LEN && id[i]; i++)
        dst[i] = (uint8_t)id[i];
}

int km_add_key(km_store_t *st, const char *id, km_role_t role,
               uint64_t material, uint64_t not_after)
{
    km_key_t *k;
    uint8_t probe[KM_ID_LEN];
    int i;
    /* IDs are fixed-width, memcmp-compared slots: a longer id would be
     * silently truncated into an alias of an existing key. */
    if (id == NULL || strlen(id) > KM_ID_LEN) return -1;
    set_id(probe, id);
    for (i = 0; i < st->n; i++) {
        if (memcmp(st->keys[i].id, probe, KM_ID_LEN) == 0) return -1;
    }
    if (st->n >= KM_MAX_KEYS) return -1;
    k = &st->keys[st->n++];
    set_id(k->id, id);
    k->role = role;
    k->material = material;
    k->not_after = not_after;
    k->revoked = 0;
    return 0;
}

const km_key_t *km_find(const km_store_t *st, const char *id)
{
    uint8_t key[KM_ID_LEN];
    int i;
    set_id(key, id);
    for (i = 0; i < st->n; i++) {
        if (memcmp(st->keys[i].id, key, KM_ID_LEN) == 0)
            return &st->keys[i];
    }
    return NULL;
}

int km_revoke(km_store_t *st, const char *id, uint64_t now)
{
    uint8_t key[KM_ID_LEN];
    int i;
    (void)now;
    set_id(key, id);
    for (i = 0; i < st->n; i++) {
        if (memcmp(st->keys[i].id, key, KM_ID_LEN) == 0) {
            st->keys[i].revoked = 1;
            return 0;
        }
    }
    return -1;
}

int km_usable(const km_key_t *k, uint64_t now)
{
    return k && !k->revoked && now <= k->not_after;
}

int km_wrap(const km_store_t *st, const char *kek_id, const char *dek_id,
            uint8_t wrapped[8], uint64_t now)
{
    const km_key_t *kek = km_find(st, kek_id);
    const km_key_t *dek = km_find(st, dek_id);
    uint64_t w;
    int i;
    if (!km_usable(kek, now) || !km_usable(dek, now)) return -1;
    if (kek->role != KM_KEK || dek->role != KM_DEK) return -1;
    w = dek->material ^ mix64_u64(kek->material);
    for (i = 0; i < 8; i++)
        wrapped[i] = (uint8_t)(w >> (8 * i));
    return 0;
}

int km_unwrap(const km_store_t *st, const char *kek_id,
              const uint8_t wrapped[8], uint64_t *dek_out, uint64_t now)
{
    const km_key_t *kek = km_find(st, kek_id);
    uint64_t w = 0;
    int i;
    if (!km_usable(kek, now) || kek->role != KM_KEK) return -1;
    for (i = 0; i < 8; i++)
        w |= (uint64_t)wrapped[i] << (8 * i);
    *dek_out = w ^ mix64_u64(kek->material);
    return 0;
}
