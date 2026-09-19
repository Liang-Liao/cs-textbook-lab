/* copied from lab14-transform-coding */
#ifndef LAB16_HUFF_H
#define LAB16_HUFF_H
#include <stddef.h>
#include <stdint.h>
/* Encode signed symbols with Huffman; returns bytes written or -1. */
int huffman_encode(const int *sym, int n, uint8_t *out, int out_cap, int *out_bits);
int huffman_decode(const uint8_t *in, int in_bytes, int *sym, int max_n);
#endif
