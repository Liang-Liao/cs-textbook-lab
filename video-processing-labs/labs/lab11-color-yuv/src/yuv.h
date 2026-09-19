#ifndef LAB11_YUV_H
#define LAB11_YUV_H

#include <stdint.h>

typedef enum {
    YUV_STD_BT601 = 0,
    YUV_STD_BT709 = 1
} yuv_std_t;

typedef struct {
    double fwd[3][3];
    double inv[3][3];
    double off_fwd[3];
    double off_inv[3];
} yuv_matrix;

typedef struct {
    int w, h;
    double *y;
    double *u;
    double *v;
} yuv420;

void yuv_matrix_init(yuv_matrix *m, yuv_std_t std);

void rgb_to_yuv(const yuv_matrix *m, double r, double g, double b,
                double *y, double *u, double *v);
void yuv_to_rgb(const yuv_matrix *m, double y, double u, double v,
                double *r, double *g, double *b);

int rgb8_to_yuv444(const yuv_matrix *m, const uint8_t *rgb, int w, int h,
                   double *y, double *u, double *v);
int yuv444_to_rgb8(const yuv_matrix *m, const double *y, const double *u,
                   const double *v, int w, int h, uint8_t *rgb);

int yuv420_alloc(yuv420 *p, int w, int h);
void yuv420_free(yuv420 *p);
int rgb8_to_yuv420(const yuv_matrix *m, const uint8_t *rgb, int w, int h, yuv420 *p);
int yuv420_to_rgb8(const yuv_matrix *m, const yuv420 *p, uint8_t *rgb);

void chroma_upsample_nearest(const double *ch, int cw, int chh, double *out, int w, int h);

#endif
