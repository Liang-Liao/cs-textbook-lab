#include "metrics.h"

#include <math.h>

double psnr_u8(const uint8_t *a, const uint8_t *b, int n) {
    double mse = 0.0;
    for (int i = 0; i < n; i++) {
        double d = (double)a[i] - (double)b[i];
        mse += d * d;
    }
    mse /= (double)n;
    if (mse < 1e-12) return 99.0;
    return 10.0 * log10(255.0 * 255.0 / mse);
}

double ssim_u8(const uint8_t *a, const uint8_t *b, int w, int h, int win) {
    if (!a || !b || w <= 2 || h <= 2) return 0.0;
    if (win < 3) win = 3;
    if (win > w || win > h) win = (w < h) ? w : h;
    if (win % 2 == 0) win--;
    if (win < 3) return 0.0;
    const double C1 = 6.5025;
    const double C2 = 58.5225;
    double acc = 0.0;
    int count = 0;
    int o = win / 2;
    for (int y = o; y + o < h; y++) {
        for (int x = o; x + o < w; x++) {
            double sa = 0, sb = 0, saa = 0, sbb = 0, sab = 0;
            int m = 0;
            for (int j = -o; j <= o; j++) {
                for (int i = -o; i <= o; i++) {
                    double va = (double)a[(y + j) * w + (x + i)];
                    double vb = (double)b[(y + j) * w + (x + i)];
                    sa += va; sb += vb; saa += va * va; sbb += vb * vb; sab += va * vb;
                    m++;
                }
            }
            if (m <= 0) continue;
            double ma = sa / m, mb = sb / m;
            double va = saa / m - ma * ma;
            double vb = sbb / m - mb * mb;
            double cov = sab / m - ma * mb;
            double num = (2.0 * ma * mb + C1) * (2.0 * cov + C2);
            double den = (ma * ma + mb * mb + C1) * (va + vb + C2);
            double s = (den > 1e-12) ? (num / den) : 0.0;
            acc += s;
            count++;
        }
    }
    if (count == 0) return 0.0;
    return acc / (double)count;
}
