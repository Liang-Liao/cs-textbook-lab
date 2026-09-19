/* copied from lab01-sampling-quantization (adapted for lab03-filters-conv) */
#include "wav_io.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void wr_u32(FILE *f, uint32_t v) {
    uint8_t b[4] = {(uint8_t)(v & 0xff), (uint8_t)((v >> 8) & 0xff),
                    (uint8_t)((v >> 16) & 0xff), (uint8_t)((v >> 24) & 0xff)};
    fwrite(b, 1, 4, f);
}

static void wr_u16(FILE *f, uint16_t v) {
    uint8_t b[2] = {(uint8_t)(v & 0xff), (uint8_t)((v >> 8) & 0xff)};
    fwrite(b, 1, 2, f);
}

int wav_write(const char *path, const wav_data *w) {
    if (!path || !w || !w->samples || w->channels == 0) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -2;
    const uint32_t byte_rate = w->sample_rate * w->channels * (w->bits_per_sample / 8);
    const uint16_t block_align = (uint16_t)(w->channels * (w->bits_per_sample / 8));
    const uint32_t data_bytes = w->num_frames * block_align;
    fwrite("RIFF", 1, 4, f);
    wr_u32(f, 36 + data_bytes);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    wr_u32(f, 16);
    wr_u16(f, 1);
    wr_u16(f, w->channels);
    wr_u32(f, w->sample_rate);
    wr_u32(f, byte_rate);
    wr_u16(f, block_align);
    wr_u16(f, w->bits_per_sample);
    fwrite("data", 1, 4, f);
    wr_u32(f, data_bytes);
    for (uint32_t i = 0; i < data_bytes / 2; i++) wr_u16(f, (uint16_t)w->samples[i]);
    int werr = ferror(f);
    fclose(f);
    return werr ? -6 : 0;
}

void wav_free(wav_data *w) {
    if (!w) return;
    free(w->samples);
    w->samples = NULL;
}

int wav_from_floats(const float *mono, size_t n, uint32_t sample_rate, wav_data *out) {
    if (!mono || !out || n == 0) return -1;
    int16_t *s = (int16_t *)malloc(n * sizeof(int16_t));
    if (!s) return -2;
    for (size_t i = 0; i < n; i++) {
        float v = mono[i];
        if (v > 0.999969f) v = 0.999969f;
        if (v < -1.0f) v = -1.0f;
        s[i] = (int16_t)lrintf(v * 32767.0f);
    }
    out->sample_rate = sample_rate;
    out->channels = 1;
    out->bits_per_sample = 16;
    out->num_frames = (uint32_t)n;
    out->samples = s;
    return 0;
}
