/* copied from lab01-sampling-quantization */
#ifndef LAB03_WAV_IO_H
#define LAB03_WAV_IO_H

#include <stdint.h>

typedef struct {
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
    uint32_t num_frames;
    int16_t *samples;
} wav_data;

int wav_write(const char *path, const wav_data *w);
void wav_free(wav_data *w);
int wav_from_floats(const float *mono, size_t n, uint32_t sample_rate, wav_data *out);

#endif
