#include "combine.h"
#include "crypto_common.h"

void double_encrypt(const uint8_t in[8], uint8_t out[8],
                    uint64_t k1, uint64_t k2)
{
    uint8_t mid[8];
    mini_encrypt_block(in, mid, &k1);
    mini_encrypt_block(mid, out, &k2);
}

void double_decrypt(const uint8_t in[8], uint8_t out[8],
                    uint64_t k1, uint64_t k2)
{
    uint8_t mid[8];
    mini_decrypt_block(in, mid, &k2);
    mini_decrypt_block(mid, out, &k1);
}

void triple_encrypt_ede(const uint8_t in[8], uint8_t out[8],
                        uint64_t k1, uint64_t k2, uint64_t k3)
{
    uint8_t a[8], b[8];
    mini_encrypt_block(in, a, &k1);
    mini_decrypt_block(a, b, &k2);   /* EDE */
    mini_encrypt_block(b, out, &k3);
}

void triple_decrypt_ede(const uint8_t in[8], uint8_t out[8],
                        uint64_t k1, uint64_t k2, uint64_t k3)
{
    uint8_t a[8], b[8];
    mini_decrypt_block(in, a, &k3);
    mini_encrypt_block(a, b, &k2);
    mini_decrypt_block(b, out, &k1);
}

typedef struct {
    uint64_t mid;
    uint64_t k1;
} mitm_ent_t;

static int mitm_ent_cmp(const void *a, const void *b)
{
    const mitm_ent_t *x = (const mitm_ent_t *)a;
    const mitm_ent_t *y = (const mitm_ent_t *)b;
    if (x->mid < y->mid) return -1;
    if (x->mid > y->mid) return 1;
    return 0;
}

static uint64_t load64le(const uint8_t p[8])
{
    uint64_t v = 0;
    int i;
    for (i = 0; i < 8; i++)
        v |= (uint64_t)p[i] << (8 * i);
    return v;
}

/* MITM: table E_k1(P) sorted by mid; lookup D_k2(C) via binary search.
 * Time ~ O(2^k log 2^k) ≈ 2^{k+1} up to log factor (educational). */
int mitm_double(const uint8_t pt[8], const uint8_t ct[8],
                int key_bits, mitm_result_t *res)
{
    uint64_t space, i, j;
    mitm_ent_t *table;

    res->found = 0;
    if (key_bits < 1 || key_bits > 16) return -1;
    space = 1ULL << key_bits;
    table = (mitm_ent_t *)malloc((size_t)space * sizeof(mitm_ent_t));
    if (!table) return -1;

    for (i = 0; i < space; i++) {
        uint64_t k = i;
        uint8_t midb[8];
        mini_encrypt_block(pt, midb, &k);
        table[i].mid = load64le(midb);
        table[i].k1  = k;
    }
    qsort(table, (size_t)space, sizeof(mitm_ent_t), mitm_ent_cmp);

    for (j = 0; j < space; j++) {
        uint64_t k2 = j;
        uint8_t midb[8];
        uint64_t key;
        mitm_ent_t *hit;
        mitm_ent_t probe;
        mini_decrypt_block(ct, midb, &k2);
        key = load64le(midb);
        probe.mid = key;
        probe.k1 = 0;
        hit = (mitm_ent_t *)bsearch(&probe, table, (size_t)space,
                                    sizeof(mitm_ent_t), mitm_ent_cmp);
        if (hit) {
            res->found = 1;
            res->k1 = hit->k1;
            res->k2 = k2;
            free(table);
            return 0;
        }
    }
    free(table);
    return 0;
}
