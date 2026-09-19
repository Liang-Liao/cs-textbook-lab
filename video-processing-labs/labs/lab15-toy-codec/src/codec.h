#ifndef LAB15_CODEC_H
#define LAB15_CODEC_H
#include <stdint.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void dct8(const double in[64], double out[64]);
void idct8(const double in[64], double out[64]);

/* Bitstream writer/reader */
typedef struct {
    uint8_t *buf;
    int cap;     /* bytes */
    int bitpos;  /* bits written/read */
} bitstream;

void bs_init(bitstream *bs, uint8_t *buf, int cap_bytes);
void bs_open(bitstream *bs, uint8_t *buf, int cap_bytes); /* no memset */
void bs_put(bitstream *bs, unsigned v, int nbits);
void bs_put_se(bitstream *bs, int v);
int  bs_get(bitstream *bs, int nbits);
int  bs_get_se(bitstream *bs);
int  bs_bytes(const bitstream *bs);

void seq_make_frame(uint8_t *img, int w, int h, int t);
double frame_psnr(const uint8_t *a, const uint8_t *b, int n);

/* Intra candidate limit for the I-frame mode search: 0 = DC only (ablation
 * for the mode-gain test), 2 = DC/horizontal/vertical full search. */
void codec_set_intra_mode_limit(int max_mode);

/* Encode I frame into bs and reconstruct into dst. Returns bits used or -1. */
int frame_i_encode(const uint8_t *src, uint8_t *dst, int w, int h, int qp,
                   bitstream *bs);
int frame_i_decode(uint8_t *dst, int w, int h, int qp, bitstream *bs);

int frame_p_encode(const uint8_t *ref, const uint8_t *src, uint8_t *dst,
                   int w, int h, int qp, bitstream *bs);
int frame_p_decode(const uint8_t *ref, uint8_t *dst, int w, int h, int qp,
                   bitstream *bs);

/* Closed-loop encode+decode of a GOP sequence.
 * target_bits>0 enables rate control. verbose prints per-frame / summary.
 * Returns average decoded PSNR or -1. rec receives decoded frames. */
double codec_sequence(const uint8_t *frames, uint8_t *rec, int w, int h, int n,
                      int gop, int base_qp, double target_bits, int verbose);

/* Residual RLE vs Huffman lossless check. Returns 0 on success. */
int lab15_residual_roundtrip(const int zz_in[64], int *out_rle_bits, int *out_huff_bits);

#endif
