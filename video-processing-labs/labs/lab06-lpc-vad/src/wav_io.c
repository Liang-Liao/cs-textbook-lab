/* copied from lab01-sampling-quantization (adapted for lab06-lpc-vad) */
#include "wav_io.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static void wr_u32(FILE *f, uint32_t v) {
    uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)};
    fwrite(b, 1, 4, f);
}
static void wr_u16(FILE *f, uint16_t v) {
    uint8_t b[2] = {(uint8_t)v, (uint8_t)(v >> 8)};
    fwrite(b, 1, 2, f);
}
int wav_write(const char *path, const wav_data *w) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    uint32_t data_bytes = w->num_frames * w->channels * (w->bits_per_sample / 8);
    fwrite("RIFF", 1, 4, f);
    wr_u32(f, 36 + data_bytes);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    wr_u32(f, 16);
    wr_u16(f, 1);
    wr_u16(f, w->channels);
    wr_u32(f, w->sample_rate);
    wr_u32(f, w->sample_rate * w->channels * (w->bits_per_sample / 8));
    wr_u16(f, (uint16_t)(w->channels * (w->bits_per_sample / 8)));
    wr_u16(f, w->bits_per_sample);
    fwrite("data", 1, 4, f);
    wr_u32(f, data_bytes);
    for (uint32_t i = 0; i < data_bytes / 2; i++) wr_u16(f, (uint16_t)w->samples[i]);
    int werr = ferror(f);
    fclose(f);
    return werr ? -6 : 0;
}
void wav_free(wav_data *w) {
    if (w) {
        free(w->samples);
        w->samples = NULL;
    }
}
int wav_from_doubles(const double *mono, size_t n, uint32_t sr, wav_data *out) {
    int16_t *s = malloc(n * sizeof(int16_t));
    if (!s) return -1;
    for (size_t i = 0; i < n; i++) {
        double v = mono[i];
        if (v > 0.999) v = 0.999;
        if (v < -1.0) v = -1.0;
        s[i] = (int16_t)lrint(v * 32767.0);
    }
    out->sample_rate = sr;
    out->channels = 1;
    out->bits_per_sample = 16;
    out->num_frames = (uint32_t)n;
    out->samples = s;
    return 0;
}
