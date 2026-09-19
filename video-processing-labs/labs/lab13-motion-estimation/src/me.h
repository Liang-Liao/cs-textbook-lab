#ifndef LAB13_ME_H
#define LAB13_ME_H

/* Gray image w*h, uint8 */
typedef struct { int w, h; unsigned char *p; } img;

img img_new(int w, int h);
void img_free(img *im);
img img_clone(img src);
void img_shift(img src, img *dst, int dx, int dy);

typedef struct { int x, y; } mv;

/* Exhaustive full-search block SAD ME over [-range,range]². Oracle accuracy. */
mv me_full(const img *ref, const img *cur, int bx, int by, int bs, int range);

/* Classic Three-Step Search (Koga 1981): step = 2^⌊log2 R⌋ … 1, 8-neighbour
 * probe each stage. Much cheaper than full search; may miss non-unimodal SAD. */
mv me_tss(const img *ref, const img *cur, int bx, int by, int bs, int range);

/* Hierarchical diamond search (large diamond → small diamond → refine). */
mv me_diamond(const img *ref, const img *cur, int bx, int by, int bs, int range);

/* Bilinear sample */
double img_sample(const img *im, double x, double y);

/* Compensate cur block with half-pel MV onto pred block bs*bs */
void mc_block(const img *ref, double mvx, double mvy, int bx, int by, int bs,
              unsigned char *out);

double sad_block(const img *a, const img *b, int bx, int by, int bs);

double wall_ms(void);

#endif
