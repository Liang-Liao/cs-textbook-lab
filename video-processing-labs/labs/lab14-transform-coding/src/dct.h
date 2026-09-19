#ifndef LAB14_DCT_H
#define LAB14_DCT_H
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
void dct8(const double in[64], double out[64]);
void idct8(const double in[64], double out[64]);
void quant8(const double coef[64], int qp, int q[64]);
void dequant8(const int q[64], int qp, double coef[64]);
/* JPEG-style perceptual quantization matrix (luminance), quality 1..100. */
void quant8_matrix(const double coef[64], int quality, int q[64]);
void dequant8_matrix(const int q[64], int quality, double coef[64]);
void zigzag(const int q[64], int zz[64]);
void unzigzag(const int zz[64], int q[64]);
#endif
