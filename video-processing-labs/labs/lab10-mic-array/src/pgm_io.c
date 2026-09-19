/* copied from lab01-sampling-quantization (adapted for lab10-mic-array) */
#include "pgm_io.h"
#include <stdio.h>
int pgm_write(const char *path, const uint8_t *p, int w, int h) {
    FILE *f=fopen(path,"wb"); if(!f) return -1;
    fprintf(f,"P5\n%d %d\n255\n",w,h);
    { size_t want=(size_t)w*(size_t)h; int werr = (fwrite(p,1,want,f) != want) || ferror(f); fclose(f); return werr ? -6 : 0; }
}
