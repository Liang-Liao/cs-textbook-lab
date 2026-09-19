#ifndef LAB14_RLE_H
#define LAB14_RLE_H

/* End-of-block marker used in the RLE symbol stream (fits Huffman range). */
#define RLE_EOB (-1000)

/* Encode one zigzag-ordered 8x8 block into a symbol stream:
 *   [DC] (run, level)* EOB
 * run ∈ [0,62] is the count of zeros before a non-zero level.
 * Returns number of symbols written, or -1 on error. */
int rle_encode_block(const int zz[64], int *out, int out_cap);

/* Decode one block from symbols. Returns symbols consumed, or -1.
 * On success zz[64] is fully populated. */
int rle_decode_block(const int *in, int in_n, int zz[64]);

#endif
