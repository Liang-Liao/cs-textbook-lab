#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dsp_tools.h"
#include "pgm_io.h"
#include "wav_io.h"

static int g_pass = 0;
static int g_fail = 0;

static void report(const char *name, double value, const char *op, double thr, int ok) {
    if (ok) {
        g_pass++;
        printf("[PASS] %s=%.6g (criterion %s %.6g)\n", name, value, op, thr);
    } else {
        g_fail++;
        printf("[FAIL] %s=%.6g (criterion %s %.6g)\n", name, value, op, thr);
    }
}

static int write_wav_floats(const char *path, const float *x, size_t n, uint32_t fs) {
    wav_data w;
    if (wav_from_floats(x, n, fs, &w) != 0) return -1;
    int rc = wav_write(path, &w);
    wav_free(&w);
    return rc;
}


static int test_wav_roundtrip(void) {
    const uint32_t fs = 16000;
    const size_t n = 3200;
    float *x = (float *)malloc(n * sizeof(float));
    if (!x) return -1;
    double f = 440.0, a = 0.5;
    synth_multitone(x, n, fs, &f, &a, 1);

    wav_data w;
    if (wav_from_floats(x, n, fs, &w) != 0) {
        free(x);
        return -1;
    }
    const char *path = "out/roundtrip.wav";
    if (wav_write(path, &w) != 0) {
        wav_free(&w);
        free(x);
        return -1;
    }
    wav_data r;
    int rc = wav_read(path, &r);
    int ok = 0;
    if (rc == 0) {
        ok = (r.sample_rate == fs && r.channels == 1 && r.bits_per_sample == 16 &&
              r.num_frames == w.num_frames);
        if (ok) {
            for (uint32_t i = 0; i < w.num_frames; i++) {
                if (r.samples[i] != w.samples[i]) {
                    ok = 0;
                    break;
                }
            }
        }
    }
    report("wav_roundtrip_mismatch", ok ? 0.0 : 1.0, "==", 0.0, ok);
    wav_free(&w);
    wav_free(&r);
    free(x);
    return ok ? 0 : -1;
}

static int test_aliasing(void) {
    /* roadmap exp 2: a MULTI-TONE signal above Nyquist — every component
     * folds back to its own alias position. */
    static const struct {
        double fs;
        double f[3];
    } cases[3] = {
        {8000.0, {4600.0, 5300.0, 6100.0}},
        {16000.0, {9200.0, 10700.0, 12100.0}},
        {44100.0, {25000.0, 28000.0, 33000.0}},
    };
    const size_t n = 8192;
    float *x = (float *)malloc(n * sizeof(float));
    if (!x) return -1;
    int all_ok = 1;
    int nfft = 512;

    for (int i = 0; i < 3; i++) {
        double fs = cases[i].fs;
        double a[3] = {0.5, 0.3, 0.2};
        synth_multitone(x, n, fs, cases[i].f, a, 3);
        char wavpath[128];
        snprintf(wavpath, sizeof(wavpath), "out/alias_%d.wav", i);
        write_wav_floats(wavpath, x, n, (uint32_t)fs);

        /* every component must appear at its folded frequency: DFT magnitude
         * at the alias bin must stand well above the local floor */
        float *mag = (float *)malloc(nfft * sizeof(float));
        if (!mag) { free(x); return -1; }
        dft_magnitude(x, nfft, mag);
        int case_ok = 1;
        for (int t = 0; t < 3; t++) {
            double expect = alias_fold(cases[i].f[t], fs);
            int bin = (int)(expect / fs * nfft + 0.5);
            if (bin >= nfft / 2) bin = nfft / 2 - 1;
            /* local floor: median magnitude over +/- 16 bins excluding peak */
            float nb[33];
            int nn = 0;
            for (int k = bin - 16; k <= bin + 16; k++) {
                if (k < 1 || k >= nfft / 2 || abs(k - bin) < 4) continue;
                nb[nn++] = mag[k];
            }
            for (int p = 1; p < nn; p++) {
                float v = nb[p];
                int q = p - 1;
                while (q >= 0 && nb[q] > v) { nb[q + 1] = nb[q]; q--; }
                nb[q + 1] = v;
            }
            float floorv = nb[nn / 2] + 1e-9f;
            double spur_db = 20.0 * log10((mag[bin] + 1e-9f) / floorv);
            printf("  info: case%d tone %.0f Hz -> alias %.1f Hz spur=%.1f dB\n",
                   i, cases[i].f[t], expect, spur_db);
            if (spur_db < 20.0) case_ok = 0;
        }
        free(mag);
        char name[64];
        snprintf(name, sizeof(name), "alias_fold_case%d", i);
        report(name, case_ok ? 1.0 : 0.0, "==", 1.0, case_ok);
        if (!case_ok) all_ok = 0;
    }
    free(x);
    return all_ok ? 0 : -1;
}

static int test_quantization(void) {
    const uint32_t fs = 48000;
    const size_t n = 96000; /* 2 s — more periods for low-bit averaging */
    float *x = (float *)malloc(n * sizeof(float));
    float *y = (float *)malloc(n * sizeof(float));
    if (!x || !y) {
        free(x);
        free(y);
        return -1;
    }
    int bits_list[] = {16, 12, 8, 4};
    int all_ok = 1;
    for (int i = 0; i < 4; i++) {
        int bits = bits_list[i];
        double step = 2.0 / (double)(1u << bits);
        /* Top reconstruction level of signed mid-tread: (2^{B-1}-1)*step = 1-step. */
        double peak = 1.0 - step;
        double *xd = (double *)malloc(n * sizeof(double));
        if (!xd) {
            all_ok = 0;
            continue;
        }
        for (size_t k = 0; k < n; k++) {
            double t = (double)k / (double)fs;
            xd[k] = peak * sin(2.0 * M_PI * 997.0 * t);
        }
        double snr = quantize_snr_db_double(xd, n, bits);
        double thr_classic = quant_snr_theory_db(bits);
        double thr = thr_classic + 20.0 * log10(peak);
        double dev = fabs(snr - thr);
        char name[64];
        snprintf(name, sizeof(name), "quant_snr_%dbit_dev", bits);
        int ok = dev < 0.5;
        printf("  info: %d-bit SNR measured=%.4f dB classic=%.4f dB theory(peak)=%.4f dB peak=%.6f\n",
               bits, snr, thr_classic, thr, peak);
        report(name, dev, "<", 0.5, ok);
        if (!ok) all_ok = 0;

        for (size_t k = 0; k < n; k++) x[k] = (float)xd[k];
        quantize_snr_db(x, n, bits, y);
        char wavpath[128];
        snprintf(wavpath, sizeof(wavpath), "out/quant_%dbit.wav", bits);
        write_wav_floats(wavpath, y, n, fs);
        free(xd);
    }

    /* roadmap exp 2 (dither): undithered 4-bit quantization error is
     * correlated with the signal (harmonic spurs stand above the floor);
     * TPDF dither decorrelates the error, trading a slightly higher floor
     * for the removal of the spurs. Metrics: |corr(error, signal)| and the
     * strongest harmonic spur above the spectral floor, before/after. */
    {
        double corr[2], spur[2];
        for (int variant = 0; variant < 2; variant++) {
            double step = 2.0 / (double)(1u << 4);
            double peak = 1.0 - step;
            for (size_t k = 0; k < n; k++)
                x[k] = (float)(peak * sin(2.0 * M_PI * 997.0 * k / (double)fs));
            if (variant == 1) apply_dither(x, n, fs, 4, 42u);
            double snr_q = quantize_snr_db(x, n, 4, y);

            double se = 0, se2 = 0, sx = 0, sx2 = 0, sxe = 0;
            for (size_t k = 0; k < n; k++) {
                double e = (double)y[k] - (double)x[k];
                double s = (double)x[k];
                se += e; se2 += e * e; sx += s; sx2 += s * s; sxe += s * e;
            }
            double ce = se / (double)n, cs = sx / (double)n;
            double ve = se2 / (double)n - ce * ce, vs = sx2 / (double)n - cs * cs;
            double cov = sxe / (double)n - ce * cs;
            corr[variant] = fabs(cov / sqrt(ve * vs + 1e-30));

            const int nfft = 8192;
            float *mag = (float *)malloc((size_t)nfft * sizeof(float));
            if (!mag) { free(x); free(y); return -1; }
            dft_magnitude(y, nfft, mag);
            int fh = (int)(997.0 / (double)fs * nfft + 0.5);
            float tmp[4096];
            int nn = 0;
            for (int k = 2; k < nfft / 2 && nn < 4096; k += 7) tmp[nn++] = mag[k];
            for (int p = 1; p < nn; p++) {
                float v = tmp[p];
                int q = p - 1;
                while (q >= 0 && tmp[q] > v) { tmp[q + 1] = tmp[q]; q--; }
                tmp[q + 1] = v;
            }
            float floor_med = tmp[nn / 2];
            spur[variant] = -200.0;
            for (int h = 2; h <= 5; h++) {
                int b = fh * h;
                if (b >= nfft / 2) break;
                double s_db = 20.0 * log10((mag[b] + 1e-9f) / (floor_med + 1e-9f));
                if (s_db > spur[variant]) spur[variant] = s_db;
            }
            free(mag);
            printf("  info: 4-bit %s: SNR=%.2f dB |corr(err,sig)|=%.3f max_harmonic_spur=%.1f dB\n",
                   variant == 0 ? "no dither " : "with dither", snr_q, corr[variant],
                   spur[variant]);
        }
        int ok = (corr[1] < 0.7 * corr[0]) && (spur[1] < spur[0]);
        report("dither_decouples_error", corr[1], "<", 0.7 * corr[0],
               corr[1] < 0.7 * corr[0]);
        report("dither_lowers_harmonic_spur_db", spur[1], "<", spur[0],
               spur[1] < spur[0]);
        if (!ok) all_ok = 0;
        write_wav_floats("out/quant_4bit_dither.wav", y, n, fs);
    }

    /* reference wav only; heavy PGM left to --generate */
    write_wav_floats("out/sine440.wav", x, n, fs);

    free(x);
    free(y);
    return all_ok ? 0 : -1;
}

static int run_selftest(void) {
    printf("=== lab01 selftest ===\n");
    if (test_wav_roundtrip() != 0) g_fail++;
    if (test_aliasing() != 0) g_fail++;
    if (test_quantization() != 0) g_fail++;
    printf("summary: %d passed, %d failed\n", g_pass, g_fail);
    if (g_fail == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    }
    printf("FAILED %d/%d\n", g_fail, g_pass + g_fail);
    return 1;
}

static int run_generate(void) {
    const uint32_t fs = 44100;
    const size_t n = 44100;
    float *x = (float *)malloc(n * sizeof(float));
    if (!x) return 1;
    double f = 440.0, a = 0.8;
    synth_multitone(x, n, fs, &f, &a, 1);
    write_wav_floats("out/sine440.wav", x, n, fs);

    /* alias demo: 7 kHz at 8 kHz sampling */
    double fa = 7000.0, aa = 1.0;
    synth_multitone(x, n, 8000, &fa, &aa, 1);
    write_wav_floats("out/alias_7k_at_8k.wav", x, n, 8000);

    float *y = (float *)malloc(n * sizeof(float));
    if (y) {
        quantize_snr_db(x, n, 8, y);
        write_wav_floats("out/quant_8bit.wav", y, n, 8000);
        free(y);
    }

    int pw = 256, ph = 128;
    uint8_t *img = (uint8_t *)calloc((size_t)pw * ph, 1);
    if (img) {
        /* small nfft: lab01 has O(N^2) DFT; FFT arrives in lab02 */
        pgm_spectrogram(x, n, 256, 32, img, pw, ph);
        pgm_write("out/spectrum_alias.pgm", img, pw, ph);
        free(img);
    }
    free(x);
    printf("generated artifacts under out/\n");
    return 0;
}

int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "--selftest")) return run_selftest();
    if (argc > 1 && !strcmp(argv[1], "--generate")) return run_generate();
    printf("usage: %s --selftest | --generate\n", argv[0]);
    return 2;
}
