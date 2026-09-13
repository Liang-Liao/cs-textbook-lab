#ifndef CGL_IMAGE_H
#define CGL_IMAGE_H

#include "cgl_vec.h"

typedef struct {
  int w, h;
  float *rgb; /* w * h * 3, linear RGB, origin top-left, y down */
} cgl_image;

cgl_image *cgl_image_create(int w, int h);
void cgl_image_free(cgl_image *img);
void cgl_image_set(cgl_image *img, int x, int y, cgl_vec3 c);
cgl_vec3 cgl_image_get(const cgl_image *img, int x, int y);
void cgl_image_fill(cgl_image *img, cgl_vec3 c);
int cgl_image_write_ppm(const cgl_image *img, const char *path, int binary);
/* Encode linear RGB to 8-bit via sRGB then write P6. */
int cgl_image_write_ppm_srgb(const cgl_image *img, const char *path);

#endif /* CGL_IMAGE_H */
