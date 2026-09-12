#include "timing.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif

int64_t clrs_now_us(void) {
#if defined(_WIN32)
  LARGE_INTEGER freq;
  LARGE_INTEGER now;
  QueryPerformanceFrequency(&freq);
  QueryPerformanceCounter(&now);
  return (int64_t)((now.QuadPart * 1000000LL) / freq.QuadPart);
#else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (int64_t)ts.tv_sec * 1000000LL + (int64_t)(ts.tv_nsec / 1000);
#endif
}

int64_t clrs_elapsed_us(int64_t start_us) { return clrs_now_us() - start_us; }
