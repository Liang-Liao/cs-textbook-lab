#ifndef LAB19_ADPCM_H
#define LAB19_ADPCM_H
/* copied from lab15, trimmed */
#include <stdint.h>
void adpcm_encode(const int16_t *pcm, int n, uint8_t *code);
void adpcm_decode(const uint8_t *code, int n, int16_t *pcm);
#endif
