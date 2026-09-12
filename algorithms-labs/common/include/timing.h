#ifndef CLRS_TIMING_H
#define CLRS_TIMING_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Monotonic microseconds since an unspecified epoch. */
int64_t clrs_now_us(void);

/* Elapsed microseconds from start_us to now. */
int64_t clrs_elapsed_us(int64_t start_us);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_TIMING_H */
