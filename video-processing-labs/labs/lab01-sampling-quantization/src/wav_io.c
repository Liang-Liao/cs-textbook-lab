#include "wav_io.h"

#include <math.h>
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

static uint32_t rd_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint16_t rd_u16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

int wav_write(const char *path, const wav_data *w) {
    if (!path || !w || !w->samples || w->channels == 0) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -2;

    const uint32_t byte_rate =
        w->sample_rate * w->channels * (w->bits_per_sample / 8);
    const uint16_t block_align = (uint16_t)(w->channels * (w->bits_per_sample / 8));
    const uint32_t data_bytes = w->num_frames * block_align;
    const uint32_t riff_size = 36 + data_bytes;

    fwrite("RIFF", 1, 4, f);
    wr_u32(f, riff_size);
    fwrite("WAVE", 1, 4, f);

    fwrite("fmt ", 1, 4, f);
    wr_u32(f, 16);
    wr_u16(f, 1); /* PCM */
    wr_u16(f, w->channels);
    wr_u32(f, w->sample_rate);
    wr_u32(f, byte_rate);
    wr_u16(f, block_align);
    wr_u16(f, w->bits_per_sample);

    fwrite("data", 1, 4, f);
    wr_u32(f, data_bytes);

    if (w->bits_per_sample == 16) {
        for (uint32_t i = 0; i < data_bytes / 2; i++) {
            int16_t s = w->samples[i];
            wr_u16(f, (uint16_t)s);
        }
    } else {
        fclose(f);
        return -3;
    }
    int werr = ferror(f); /* any earlier fwrite failure (e.g. disk full) */
    fclose(f);
    return werr ? -6 : 0;
}

int wav_read(const char *path, wav_data *w) {
    if (!path || !w) return -1;
    memset(w, 0, sizeof(*w));
    FILE *f = fopen(path, "rb");
    if (!f) return -2;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 44) {
        fclose(f);
        return -3;
    }
    uint8_t *buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) {
        fclose(f);
        return -4;
    }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return -5;
    }
    fclose(f);

    if (memcmp(buf, "RIFF", 4) || memcmp(buf + 8, "WAVE", 4)) {
        free(buf);
        return -6;
    }

    size_t off = 12;
    uint16_t audio_format = 0, channels = 0, bits = 0, block_align = 0;
    uint32_t sample_rate = 0, data_bytes = 0;
    const uint8_t *data_ptr = NULL;

    while (off + 8 <= (size_t)sz) {
        char id[5] = {0};
        memcpy(id, buf + off, 4);
        uint32_t chunk = rd_u32(buf + off + 4);
        size_t body = off + 8;
        if (body + chunk > (size_t)sz) break;
        if (!memcmp(id, "fmt ", 4) && chunk >= 16) {
            audio_format = rd_u16(buf + body);
            channels = rd_u16(buf + body + 2);
            sample_rate = rd_u32(buf + body + 4);
            block_align = rd_u16(buf + body + 12);
            bits = rd_u16(buf + body + 14);
        } else if (!memcmp(id, "data", 4)) {
            data_bytes = chunk;
            data_ptr = buf + body;
        }
        off = body + chunk + (chunk & 1);
    }

    if (audio_format != 1 || bits != 16 || channels == 0 || !data_ptr) {
        free(buf);
        return -7;
    }

    uint32_t n_samp = data_bytes / 2;
    int16_t *s = (int16_t *)malloc(n_samp * sizeof(int16_t));
    if (!s) {
        free(buf);
        return -8;
    }
    for (uint32_t i = 0; i < n_samp; i++) {
        s[i] = (int16_t)rd_u16(data_ptr + i * 2);
    }
    free(buf);

    w->sample_rate = sample_rate;
    w->channels = channels;
    w->bits_per_sample = bits;
    w->num_frames = n_samp / channels;
    w->samples = s;
    (void)block_align;
    return 0;
}

void wav_free(wav_data *w) {
    if (!w) return;
    free(w->samples);
    w->samples = NULL;
    w->num_frames = 0;
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
