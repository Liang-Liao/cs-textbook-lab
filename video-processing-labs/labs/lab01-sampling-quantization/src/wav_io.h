#ifndef LAB01_WAV_IO_H
#define LAB01_WAV_IO_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
    uint32_t num_frames; /* frames = samples / channels */
    int16_t *samples;    /* interleaved, caller frees via wav_free */
} wav_data;

int wav_write(const char *path, const wav_data *w);
int wav_read(const char *path, wav_data *w);
void wav_free(wav_data *w);

/* Allocate interleaved s16 buffer and fill from float in [-1,1). */
int wav_from_floats(const float *mono, size_t n, uint32_t sample_rate, wav_data *out);

#endif
