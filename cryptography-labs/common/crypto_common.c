#include "crypto_common.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#endif

void hex_dump(const char *tag, const uint8_t *data, size_t len)
{
    size_t i;
    printf("%s (%zu bytes):\n  ", tag, len);
    for (i = 0; i < len; i++) {
        printf("%02x", data[i]);
        if ((i + 1) % 16 == 0 && i + 1 < len)
            printf("\n  ");
        else if ((i + 1) % 2 == 0)
            printf(" ");
    }
    printf("\n");
}

void bytes_to_hex(const uint8_t *data, size_t len, char *out)
{
    static const char *hexd = "0123456789abcdef";
    size_t i;
    for (i = 0; i < len; i++) {
        out[i * 2]     = hexd[(data[i] >> 4) & 0xF];
        out[i * 2 + 1] = hexd[data[i] & 0xF];
    }
    out[len * 2] = '\0';
}

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int hex_to_bytes(const char *hex, uint8_t *out, size_t max_out, size_t *out_len)
{
    size_t n = 0;
    while (*hex) {
        int hi, lo;
        if (*hex == ' ' || *hex == '\n' || *hex == '\t') { hex++; continue; }
        hi = hex_nibble(*hex++);
        if (hi < 0) return -1;
        if (!*hex) return -1;
        lo = hex_nibble(*hex++);
        if (lo < 0) return -1;
        if (n >= max_out) return -1;
        out[n++] = (uint8_t)((hi << 4) | lo);
    }
    if (out_len) *out_len = n;
    return 0;
}

void xor_bytes(uint8_t *dst, const uint8_t *a, const uint8_t *b, size_t len)
{
    size_t i;
    for (i = 0; i < len; i++) dst[i] = (uint8_t)(a[i] ^ b[i]);
}

int memeq_const(const uint8_t *a, const uint8_t *b, size_t len)
{
    uint8_t acc = 0;
    size_t i;
    for (i = 0; i < len; i++) acc |= (uint8_t)(a[i] ^ b[i]);
    return acc == 0;
}

int crypto_random_bytes(uint8_t *buf, size_t len)
{
#ifdef _WIN32
    NTSTATUS st = BCryptGenRandom(NULL, buf, (ULONG)len, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return (st == 0) ? 0 : -1;
#else
    FILE *f = fopen("/dev/urandom", "rb");
    size_t n;
    if (!f) return -1;
    n = fread(buf, 1, len, f);
    fclose(f);
    return (n == len) ? 0 : -1;
#endif
}

uint64_t mod_mul_u64(uint64_t a, uint64_t b, uint64_t m)
{
    if (m == 0) return 0;
#if defined(__SIZEOF_INT128__)
    return (uint64_t)(((__uint128_t)(a % m) * (b % m)) % m);
#else
    {
        /* Russian peasant with overflow-safe reduction (still educational). */
        uint64_t res = 0;
        a %= m;
        b %= m;
        while (b) {
            if (b & 1) {
                res += a;
                if (res >= m) res -= m;
            }
            if (a >= m - a) a = (a + a) - m; /* 2a mod m without wrap */
            else a <<= 1;
            b >>= 1;
        }
        return res;
    }
#endif
}

uint64_t mod_pow_u64(uint64_t base, uint64_t exp, uint64_t m)
{
    uint64_t r = 1;
    if (m == 0) return 0;
    if (m == 1) return 0;
    base %= m;
    while (exp) {
        if (exp & 1) r = mod_mul_u64(r, base, m);
        base = mod_mul_u64(base, base, m);
        exp >>= 1;
    }
    return r;
}

int mod_inv_u64(uint64_t a, uint64_t m, uint64_t *inv)
{
    /* Extended Euclidean algorithm, coefficients kept reduced mod m so the
     * q*newt product never overflows: works for the full uint64 range. */
    uint64_t t = 0, newt = 1;
    uint64_t r, newr;
    if (m == 0) return -1;
    r = m;
    newr = a % m;
    while (newr != 0) {
        uint64_t q = r / newr;
        uint64_t sub, tmp;
        sub = mod_mul_u64(q % m, newt, m);
        tmp = newt;
        /* newt = (t - sub) mod m, without t+m overflow for m near 2^64 */
        newt = (t >= sub) ? (t - sub) : (m - (sub - t));
        t = tmp;
        tmp = newr;
        newr = r - q * newr;
        r = tmp;
    }
    if (r > 1) return -1; /* not invertible */
    *inv = t;
    return 0;
}

uint64_t gcd_u64(uint64_t a, uint64_t b)
{
    while (b) {
        uint64_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}

int ext_gcd_u64(int64_t a, int64_t b, int64_t *x, int64_t *y, int64_t *g)
{
    if (b == 0) {
        *x = 1;
        *y = 0;
        *g = a;
        return 0;
    }
    {
        int64_t x1, y1, g1;
        ext_gcd_u64(b, a % b, &x1, &y1, &g1);
        *x = y1;
        *y = x1 - (a / b) * y1;
        *g = g1;
    }
    return 0;
}

static int miller_witness(uint64_t n, uint64_t d, int r, uint64_t a)
{
    uint64_t x = mod_pow_u64(a, d, n);
    int i;
    if (x == 1 || x == n - 1) return 0;
    for (i = 1; i < r; i++) {
        x = mod_mul_u64(x, x, n);
        if (x == n - 1) return 0;
    }
    return 1;
}

int is_prime_u64(uint64_t n, int rounds)
{
    static const uint64_t small[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37};
    uint64_t d;
    int r = 0, i;
    uint8_t rnd[8];
    uint64_t a;

    if (n < 2) return 0;
    for (i = 0; i < (int)(sizeof(small) / sizeof(small[0])); i++) {
        if (n == small[i]) return 1;
        if (n % small[i] == 0) return 0;
    }
    d = n - 1;
    while ((d & 1) == 0) {
        d >>= 1;
        r++;
    }
    if (rounds < 1) rounds = 8;
    for (i = 0; i < rounds; i++) {
        if (crypto_random_bytes(rnd, sizeof(rnd)) != 0) return 0;
        memcpy(&a, rnd, sizeof(a));
        a = 2 + (a % (n - 3));
        if (miller_witness(n, d, r, a)) return 0;
    }
    return 1;
}

int random_prime_u64(uint64_t lo, uint64_t hi, int miller_rounds, uint64_t *out)
{
    uint64_t span, cand;
    int tries;
    if (hi <= lo || hi < 3) return -1;
    /* 2 is the only even prime; take it explicitly when in range (the odd
     * sampling below can never produce it, and [2,3) has no odd candidate). */
    if (lo <= 2) {
        *out = 2;
        return 0;
    }
    if ((lo & 1) == 0) lo++;
    if (lo >= hi) return -1;
    span = (hi - lo) / 2 + 1; /* number of odd candidates in [lo, hi) */
    for (tries = 0; tries < 100000; tries++) {
        uint8_t rnd[8];
        uint64_t raw;
        if (crypto_random_bytes(rnd, sizeof(rnd)) != 0) return -1;
        memcpy(&raw, rnd, sizeof(raw));
        cand = lo + 2 * (raw % span);
        if (cand >= hi) {
            cand = hi - 1;
            if ((cand & 1) == 0) cand--;
        }
        if (cand < lo) continue;
        if (is_prime_u64(cand, miller_rounds)) {
            *out = cand;
            return 0;
        }
    }
    return -1;
}

uint64_t mix64_u64(uint64_t x)
{
    x ^= x >> 30;
    x *= 0xBF58476D1CE4E5B9ULL;
    x ^= x >> 27;
    x *= 0x94D049BB133111EBULL;
    x ^= x >> 31;
    return x;
}

void test_begin(test_stats_t *s, const char *suite)
{
    s->suite = suite;
    s->passed = 0;
    s->failed = 0;
    printf("=== %s ===\n", suite);
}

void test_check(test_stats_t *s, const char *name, int cond)
{
    if (cond) {
        s->passed++;
        printf("  [PASS] %s\n", name);
    } else {
        s->failed++;
        printf("  [FAIL] %s\n", name);
    }
}

void test_check_eq_hex(test_stats_t *s, const char *name,
                       const uint8_t *got, size_t got_len,
                       const uint8_t *want, size_t want_len)
{
    char g[512], w[512];
    if (got_len == want_len && memeq_const(got, want, got_len)) {
        s->passed++;
        printf("  [PASS] %s\n", name);
        return;
    }
    s->failed++;
    printf("  [FAIL] %s\n", name);
    bytes_to_hex(got, got_len < 32 ? got_len : 32, g);
    bytes_to_hex(want, want_len < 32 ? want_len : 32, w);
    printf("         got  = %s%s\n", g, got_len > 32 ? "..." : "");
    printf("         want = %s%s\n", w, want_len > 32 ? "..." : "");
}

int test_end(test_stats_t *s)
{
    printf("--- %s: %d passed, %d failed ---\n\n", s->suite, s->passed, s->failed);
    return s->failed == 0 ? 0 : 1;
}
