#ifndef LAB19_WAV_H
#define LAB19_WAV_H
/* copied from lab09, trimmed */
#include <stddef.h>
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
int wav_from_doubles(const double *mono, size_t n, uint32_t sr, wav_data *out);
#endif
