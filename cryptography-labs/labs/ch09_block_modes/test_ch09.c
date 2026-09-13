#include "crypto_common.h"
#include "modes.h"
#include "toy_cipher.h"

/* This lab is independent: modes are exercised with the local toy Feistel.
 * DES-based mode checks live in labs/ch12_des (cipher) and can be linked
 * locally there if desired — not required here. */

static const uint8_t key8[8] = {1,2,3,4,5,6,7,8};
static const uint8_t iv8[8]  = {0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77};

int main(void)
{
    test_stats_t s;
    uint8_t pt[24], ct[24], rt[24];
    size_t i;

    test_begin(&s, "Chapter 9: Block Cipher Modes");

    for (i = 0; i < sizeof(pt); i++) pt[i] = (uint8_t)(0xA0 + i);

    test_check(&s, "toy ECB roundtrip",
               mode_ecb_encrypt(pt, ct, sizeof(pt), toy_encrypt_block, key8) == 0 &&
               mode_ecb_decrypt(ct, rt, sizeof(pt), toy_decrypt_block, key8) == 0 &&
               memcmp(pt, rt, sizeof(pt)) == 0);

    memcpy(pt + 8, pt, 8);
    test_check(&s, "ECB equal blocks leak",
               mode_ecb_encrypt(pt, ct, sizeof(pt), toy_encrypt_block, key8) == 0 &&
               memcmp(ct, ct + 8, 8) == 0);
    for (i = 0; i < sizeof(pt); i++) pt[i] = (uint8_t)(0xA0 + i);

    test_check(&s, "toy CBC roundtrip",
               mode_cbc_encrypt(pt, ct, sizeof(pt), iv8, toy_encrypt_block, key8) == 0 &&
               mode_cbc_decrypt(ct, rt, sizeof(pt), iv8, toy_decrypt_block, key8) == 0 &&
               memcmp(pt, rt, sizeof(pt)) == 0);
    test_check(&s, "CBC equal blocks differ", memcmp(ct, ct + 8, 8) != 0);

    mode_cfb8_encrypt(pt, ct, sizeof(pt), iv8, toy_encrypt_block, key8);
    mode_cfb8_decrypt(ct, rt, sizeof(pt), iv8, toy_encrypt_block, key8);
    test_check(&s, "toy CFB8 roundtrip", memcmp(pt, rt, sizeof(pt)) == 0);

    mode_ofb(pt, ct, sizeof(pt), iv8, toy_encrypt_block, key8);
    mode_ofb(ct, rt, sizeof(pt), iv8, toy_encrypt_block, key8);
    test_check(&s, "toy OFB involution", memcmp(pt, rt, sizeof(pt)) == 0);

    mode_ctr(pt, ct, sizeof(pt), iv8, toy_encrypt_block, key8);
    mode_ctr(ct, rt, sizeof(pt), iv8, toy_encrypt_block, key8);
    test_check(&s, "toy CTR involution", memcmp(pt, rt, sizeof(pt)) == 0);

    /* Input validation: block modes reject lengths that are not a multiple
     * of the block size instead of silently running past the buffer. */
    test_check(&s, "ECB rejects len%8", mode_ecb_encrypt(pt, ct, 5, toy_encrypt_block, key8) == -1);
    test_check(&s, "CBC rejects len%8", mode_cbc_encrypt(pt, ct, 5, iv8, toy_encrypt_block, key8) == -1);

    /* In-place operation: CFB8 decrypt with in == out. */
    mode_cfb8_encrypt(pt, ct, sizeof(pt), iv8, toy_encrypt_block, key8);
    memcpy(rt, ct, sizeof(ct));
    mode_cfb8_decrypt(rt, rt, sizeof(rt), iv8, toy_encrypt_block, key8);
    test_check(&s, "CFB8 in-place decrypt", memcmp(pt, rt, sizeof(pt)) == 0);

    /* CBC error propagation: flip one ciphertext bit */
    {
        uint8_t c2[24], p2[24];
        int same_next, rc;
        rc = mode_cbc_encrypt(pt, c2, sizeof(pt), iv8, toy_encrypt_block, key8);
        c2[0] ^= 0x01;
        rc |= mode_cbc_decrypt(c2, p2, sizeof(pt), iv8, toy_decrypt_block, key8);
        test_check(&s, "CBC encrypt/decrypt accepted", rc == 0);
        test_check(&s, "CBC flip corrupts block 0", memcmp(p2, pt, 8) != 0);
        same_next = ((p2[8] ^ pt[8]) == 0x01) && (memcmp(p2 + 9, pt + 9, 7) == 0);
        test_check(&s, "CBC flip propagates 1 bit to next block", same_next);
    }

    return test_end(&s);
}
