#include "crypto_common.h"
#include "pkcs.h"

int main(void)
{
    test_stats_t s;
    test_begin(&s, "Chapter 20: PKCS");

    /* PKCS#7 pad/unpad */
    {
        uint8_t in[5] = {'h','e','l','l','o'};
        uint8_t out[16], back[16];
        size_t olen = 0, blen = 0;
        test_check(&s, "pad 5->8", pkcs7_pad(in, 5, 8, out, 16, &olen) == 0 && olen == 8);
        test_check(&s, "pad bytes", out[5]==3 && out[6]==3 && out[7]==3);
        test_check(&s, "unpad", pkcs7_unpad(out, olen, 8, back, 16, &blen) == 0 &&
                   blen == 5 && memcmp(back, in, 5) == 0);
        /* full block of padding when already aligned */
        test_check(&s, "aligned adds full block",
                   pkcs7_pad(in, 4, 8, out, 16, &olen) == 0 && olen == 8 && out[7] == 4);
        /* reject bad pad */
        out[7] = 9;
        test_check(&s, "reject bad pad byte",
                   pkcs7_unpad(out, 8, 8, back, 16, &blen) != 0);
        /* more malformed-input negatives */
        {
            uint8_t good[8] = {'a','b','c','d','e',3,3,3};
            uint8_t bad1[8] = {'a','b','c','d','e',3,3,0};   /* pad byte 0 */
            uint8_t bad2[8] = {'a','b','c','d','e',3,3,9};   /* pad > block */
            uint8_t bad3[8] = {'a','b','c','d','e',3,2,3};   /* inconsistent */
            test_check(&s, "reject empty input", pkcs7_unpad(good, 0, 8, back, 16, &blen) != 0);
            test_check(&s, "reject non-multiple len", pkcs7_unpad(good, 7, 8, back, 16, &blen) != 0);
            test_check(&s, "reject pad>block", pkcs7_unpad(bad2, 8, 8, back, 16, &blen) != 0);
            test_check(&s, "reject pad==0", pkcs7_unpad(bad1, 8, 8, back, 16, &blen) != 0);
            test_check(&s, "reject inconsistent pad", pkcs7_unpad(bad3, 8, 8, back, 16, &blen) != 0);
            test_check(&s, "reject out_cap too small",
                       pkcs7_unpad(good, 8, 8, back, 4, &blen) != 0);
            test_check(&s, "accept consistent pad",
                       pkcs7_unpad(good, 8, 8, back, 16, &blen) == 0 && blen == 5);
        }
    }

    /* PBKDF2 RFC 7914 / common test:
     * P="password", S="salt", c=1, dkLen=32
     * 120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b */
    {
        uint8_t dk[32];
        char hex[65];
        test_check(&s, "pbkdf2 c=1 runs",
                   pbkdf2_sha256((const uint8_t *)"password", 8,
                                 (const uint8_t *)"salt", 4, 1, dk, 32) == 0);
        bytes_to_hex(dk, 32, hex);
        test_check(&s, "pbkdf2 c=1 vector",
                   strcmp(hex, "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b") == 0);
    }

    {
        uint8_t dk[32];
        char hex[65];
        test_check(&s, "pbkdf2 c=2 runs",
                   pbkdf2_sha256((const uint8_t *)"password", 8,
                                 (const uint8_t *)"salt", 4, 2, dk, 32) == 0);
        bytes_to_hex(dk, 32, hex);
        test_check(&s, "pbkdf2 c=2 vector",
                   strcmp(hex, "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43") == 0);
    }

    /* PBKDF2 c=4096 (multi-iteration) and dkLen=40 (multi output block T_1|T_2,
     * exercises block_index >= 2 and the big-endian INT(i) suffix). */
    {
        uint8_t dk[40];
        char hex[81];
        test_check(&s, "pbkdf2 c=4096 runs",
                   pbkdf2_sha256((const uint8_t *)"password", 8,
                                 (const uint8_t *)"salt", 4, 4096, dk, 32) == 0);
        bytes_to_hex(dk, 32, hex);
        test_check(&s, "pbkdf2 c=4096 vector",
                   strcmp(hex, "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a") == 0);
        test_check(&s, "pbkdf2 dkLen=40 runs",
                   pbkdf2_sha256((const uint8_t *)"passwordPASSWORDpassword", 24,
                                 (const uint8_t *)"saltSALTsaltSALTsaltSALTsaltSALTsalt", 36,
                                 4096, dk, 40) == 0);
        bytes_to_hex(dk, 40, hex);
        test_check(&s, "pbkdf2 dkLen=40 vector",
                   strcmp(hex, "348c89dbcbd32b2f32d814b8116e84cf2b17347"
                               "ebc1800181c4e2a1fb8dd53e1c635518c7dac47e9") == 0);
    }

    /* HMAC-SHA256 direct vectors (RFC 4231 test cases 1 and 2). */
    {
        uint8_t mac[32];
        char hex[65];
        hmac_sha256((const uint8_t *)"\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b"
                                     "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b", 20,
                    (const uint8_t *)"Hi There", 8, mac);
        bytes_to_hex(mac, 32, hex);
        test_check(&s, "HMAC-SHA256 RFC4231 #1",
                   strcmp(hex, "b0344c61d8db38535ca8afceaf0bf12b"
                               "881dc200c9833da726e9376c2e32cff7") == 0);
        hmac_sha256((const uint8_t *)"Jefe", 4,
                    (const uint8_t *)"what do ya want for nothing?", 28, mac);
        bytes_to_hex(mac, 32, hex);
        test_check(&s, "HMAC-SHA256 RFC4231 #2",
                   strcmp(hex, "5bdcc146bf60754e6a042426089575c7"
                               "5a003f089d2739839dec58b964ec3843") == 0);
    }

    return test_end(&s);
}
