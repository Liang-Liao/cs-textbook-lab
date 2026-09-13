#include "cgl_image.h"

#include <stdio.h>
#include <stdlib.h>

#include "cgl_color.h"

cgl_image *cgl_image_create(int w, int h) {
  if (w <= 0 || h <= 0) {
    return NULL;
  }
  cgl_image *img = (cgl_image *)calloc(1, sizeof(cgl_image));
  if (!img) {
    return NULL;
  }
  img->w = w;
  img->h = h;
  img->rgb = (float *)calloc((size_t)w * (size_t)h * 3u, sizeof(float));
  if (!img->rgb) {
    free(img);
    return NULL;
  }
  return img;
}

void cgl_image_free(cgl_image *img) {
  if (!img) {
    return;
  }
  free(img->rgb);
  free(img);
}

void cgl_image_set(cgl_image *img, int x, int y, cgl_vec3 c) {
  if (!img || x < 0 || y < 0 || x >= img->w || y >= img->h) {
    return;
  }
  size_t i = ((size_t)y * (size_t)img->w + (size_t)x) * 3u;
  img->rgb[i + 0] = c.x;
  img->rgb[i + 1] = c.y;
  img->rgb[i + 2] = c.z;
}

cgl_vec3 cgl_image_get(const cgl_image *img, int x, int y) {
  if (!img || x < 0 || y < 0 || x >= img->w || y >= img->h) {
    return cgl_v3(0, 0, 0);
  }
  size_t i = ((size_t)y * (size_t)img->w + (size_t)x) * 3u;
  return cgl_v3(img->rgb[i], img->rgb[i + 1], img->rgb[i + 2]);
}

void cgl_image_fill(cgl_image *img, cgl_vec3 c) {
  if (!img) {
    return;
  }
  for (int y = 0; y < img->h; ++y) {
    for (int x = 0; x < img->w; ++x) {
      cgl_image_set(img, x, y, c);
    }
  }
}

static unsigned char cgl_to_u8(float v) {
  if (!(v > 0.0f)) { /* also maps NaN to 0: NaN fails every comparison */
    return 0;
  }
  if (v >= 1.0f) {
    return 255;
  }
  return (unsigned char)(v * 255.0f + 0.5f);
}

int cgl_image_write_ppm(const cgl_image *img, const char *path, int binary) {
  if (!img || !path) {
    return -1;
  }
  FILE *f = fopen(path, "wb");
  if (!f) {
    return -1;
  }
  int ok = fprintf(f, "%s\n%d %d\n255\n", binary ? "P6" : "P3", img->w,
                   img->h) > 0;
  for (int y = 0; ok && y < img->h; ++y) {
    for (int x = 0; ok && x < img->w; ++x) {
      cgl_vec3 c = cgl_image_get(img, x, y);
      unsigned char px[3] = {cgl_to_u8(c.x), cgl_to_u8(c.y), cgl_to_u8(c.z)};
      if (binary) {
        ok = fwrite(px, 1, 3, f) == 3;
      } else {
        ok = fprintf(f, "%u %u %u\n", px[0], px[1], px[2]) > 0;
      }
    }
  }
  if (fclose(f) != 0) {
    ok = 0;
  }
  return ok ? 0 : -1;
}

int cgl_image_write_ppm_srgb(const cgl_image *img, const char *path) {
  if (!img || !path) {
    return -1;
  }
  FILE *f = fopen(path, "wb");
  if (!f) {
    return -1;
  }
  int ok = fprintf(f, "P6\n%d %d\n255\n", img->w, img->h) > 0;
  for (int y = 0; ok && y < img->h; ++y) {
    for (int x = 0; ok && x < img->w; ++x) {
      cgl_vec3 lin = cgl_image_get(img, x, y);
      cgl_vec3 s = cgl_linear_to_srgb(lin);
      unsigned char px[3] = {cgl_to_u8(s.x), cgl_to_u8(s.y), cgl_to_u8(s.z)};
      ok = fwrite(px, 1, 3, f) == 3;
    }
  }
  if (fclose(f) != 0) {
    ok = 0;
  }
  return ok ? 0 : -1;
}
