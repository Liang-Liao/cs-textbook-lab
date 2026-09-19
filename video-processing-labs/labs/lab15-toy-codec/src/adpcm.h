#ifndef LAB15_ADPCM_H
#define LAB15_ADPCM_H
#include <stdint.h>
void adpcm_encode(const int16_t *pcm, int n, uint8_t *code /* 4bit packed */);
void adpcm_decode(const uint8_t *code, int n, int16_t *pcm);
double adpcm_snr_db(const int16_t *ref, const int16_t *dec, int n);
#endif
