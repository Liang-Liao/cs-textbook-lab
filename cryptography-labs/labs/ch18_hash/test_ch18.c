#include "crypto_common.h"
#include "md5.h"
#include "sha1.h"
#include "sha256.h"

static int hexeq(const uint8_t *got, size_t n, const char *want_hex)
{
    char g[128];
    size_t i;
    if (n * 2 >= sizeof(g)) return 0;
    bytes_to_hex(got, n, g);
    for (i = 0; g[i] && want_hex[i]; i++) {
        char a = g[i], b = want_hex[i];
        if (a >= 'A' && a <= 'F') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'F') b = (char)(b - 'A' + 'a');
        if (a != b) return 0;
    }
    return g[i] == '\0' && want_hex[i] == '\0';
}

int main(void)
{
    test_stats_t s;
    test_begin(&s, "Chapter 18: One-Way Hash Functions");

    /* MD5 RFC 1321 test vectors */
    {
        uint8_t d[16];
        md5((const uint8_t *)"", 0, d);
        test_check(&s, "MD5(\"\")", hexeq(d, 16, "d41d8cd98f00b204e9800998ecf8427e"));
        md5((const uint8_t *)"a", 1, d);
        test_check(&s, "MD5(\"a\")", hexeq(d, 16, "0cc175b9c0f1b6a831c399e269772661"));
        md5((const uint8_t *)"abc", 3, d);
        test_check(&s, "MD5(\"abc\")", hexeq(d, 16, "900150983cd24fb0d6963f7d28e17f72"));
        md5((const uint8_t *)"message digest", 14, d);
        test_check(&s, "MD5(message digest)", hexeq(d, 16, "f96b697d7cb7938d525a2f31aaf161d0"));
        md5((const uint8_t *)"abcdefghijklmnopqrstuvwxyz", 26, d);
        test_check(&s, "MD5(alphabet, multi-block)",
                   hexeq(d, 16, "c3fcd3d76192e4007dfb496cca67e13b"));
    }

    /* SHA-1 FIPS test vectors */
    {
        uint8_t d[20];
        sha1((const uint8_t *)"abc", 3, d);
        test_check(&s, "SHA1(abc)", hexeq(d, 20, "a9993e364706816aba3e25717850c26c9cd0d89d"));
        sha1((const uint8_t *)"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56, d);
        test_check(&s, "SHA1(2-block)", hexeq(d, 20, "84983e441c3bd26ebaae4aa1f95129e5e54670f1"));
        sha1((const uint8_t *)"", 0, d);
        test_check(&s, "SHA1(\"\")", hexeq(d, 20, "da39a3ee5e6b4b0d3255bfef95601890afd80709"));
    }

    /* SHA-256 FIPS vectors */
    {
        uint8_t d[32];
        sha256((const uint8_t *)"abc", 3, d);
        test_check(&s, "SHA256(abc)", hexeq(d, 32,
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
        sha256((const uint8_t *)"", 0, d);
        test_check(&s, "SHA256(\"\")", hexeq(d, 32,
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
        sha256((const uint8_t *)"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56, d);
        test_check(&s, "SHA256(2-block)", hexeq(d, 32,
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
    }

    /* Padding-boundary lengths: message = 'A'..'Z' repeating. Lengths 55/56
     * exercise the "extra length block" boundary, 63/64 the exact-block case,
     * 111/119/120 multi-block + boundary. Digests generated independently. */
    {
        static const struct {
            int len;
            const char *md5, *sha1, *sha256;
        } tv[] = {
            {55,  "bdf2ee53fac2a42e4692411a8ffcd4e2",
                  "ebd854f0c7c9f58a1d5af5dee2b4c039902f945b",
                  "5be2d480ff0ea4094fb05e7adb2c1dbf1d7bd99dec429827ea4b9c1ddc7d3bf1"},
            {56,  "ca240139f53edd0770b4a912d4789bba",
                  "45fe53c317500145812034ecf0061f12af48d782",
                  "f257b5e23c5cb81d044c7a9074ceeac9235ff5c1b377048c7c368ef1038462a2"},
            {63,  "d431e699bf5b185ebd51ba55f6f70f6b",
                  "e35dbc73b66138acbbbf224729361960c2507b5b",
                  "8abf253c52140a4203ed43a606808e342ec84856c5c32bb8e4e3869885f83534"},
            {64,  "cebf5ced0a0d39bfedab859f9445f6ca",
                  "220faa7c1778bd2cc286c8b9ccec03e1131b52d8",
                  "9b0c3567683f6d5198d714b00c40df3039f7717ba5ca65c2b6617688b63b42fa"},
            {111, "aef15888ccd0d412724e3135b95a62c2",
                  "544b38231f40b866c49ca66de9c07f729f984255",
                  "067ddd33c5a405b89b0994aaca1154b0d77b0c7cc09efe50a1797565479540da"},
            {119, "6f7faa84f406af9ade81c4a84605bf03",
                  "7c45a39ccc9d19373773c33d8e9d9d2643fbd8c8",
                  "774836b19af4f3015a8dff4e9cb353b11899645ade4626c677a21c84cdb00050"},
            {120, "1418ea1f3b7586341fbe33071d792965",
                  "3177f24a0f2a5c69c1c3dfd44b6bb25609930636",
                  "a26af5006eb83a2fc03fcd4a54ff5ab7269b4732bb84dc76c41ec097ce17be3f"},
        };
        uint8_t m[120], d16[16], d20[20], d32[32];
        char name[64];
        size_t i;
        int k;
        for (i = 0; i < sizeof(m); i++) m[i] = (uint8_t)('A' + (i % 26));
        for (k = 0; k < (int)(sizeof(tv) / sizeof(tv[0])); k++) {
            md5(m, (size_t)tv[k].len, d16);
            sha1(m, (size_t)tv[k].len, d20);
            sha256(m, (size_t)tv[k].len, d32);
            sprintf(name, "boundary %d: md5/sha1/sha256", tv[k].len);
            test_check(&s, name,
                       hexeq(d16, 16, tv[k].md5) &&
                       hexeq(d20, 20, tv[k].sha1) &&
                       hexeq(d32, 32, tv[k].sha256));
        }
    }

    /* Streaming path: same 120-byte message fed in three chunks. */
    {
        uint8_t m[120];
        uint8_t d5a[16], d5b[16], d1a[20], d1b[20], d2a[32], d2b[32];
        md5_ctx_t c5;
        sha1_ctx_t c1;
        sha256_ctx_t c2;
        size_t i;
        for (i = 0; i < sizeof(m); i++) m[i] = (uint8_t)('A' + (i % 26));
        md5(m, sizeof(m), d5a);
        sha1(m, sizeof(m), d1a);
        sha256(m, sizeof(m), d2a);
        md5_init(&c5);
        md5_update(&c5, m, 40);
        md5_update(&c5, m + 40, 50);
        md5_update(&c5, m + 90, 30);
        md5_final(&c5, d5b);
        sha1_init(&c1);
        sha1_update(&c1, m, 40);
        sha1_update(&c1, m + 40, 50);
        sha1_update(&c1, m + 90, 30);
        sha1_final(&c1, d1b);
        sha256_init(&c2);
        sha256_update(&c2, m, 40);
        sha256_update(&c2, m + 40, 50);
        sha256_update(&c2, m + 90, 30);
        sha256_final(&c2, d2b);
        test_check(&s, "streaming equals one-shot",
                   memcmp(d5a, d5b, 16) == 0 &&
                   memcmp(d1a, d1b, 20) == 0 &&
                   memcmp(d2a, d2b, 32) == 0);
    }

    /* avalanche */
    {
        uint8_t h1[32], h2[32];
        sha256((const uint8_t *)"hello", 5, h1);
        sha256((const uint8_t *)"hellp", 5, h2);
        test_check(&s, "SHA256 avalanche", memcmp(h1, h2, 32) != 0);
    }

    return test_end(&s);
}
