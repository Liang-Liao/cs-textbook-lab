#include "number_theory.h"

#include "clrs.h"

/* (a * b) mod m, 0 <= a,b, a*b fits when using additive doubling. */
static int64_t mul_mod(int64_t a, int64_t b, int64_t m) {
  int64_t result = 0;
  int64_t x = a % m;
  int64_t y = b;
  if (x < 0) {
    x += m;
  }
  if (y < 0) {
    y = -y;
  }
  while (y > 0) {
    if (y & 1) {
      result = (result + x) % m;
    }
    x = (x * 2) % m;
    y >>= 1;
  }
  return result;
}

/*
 * CLRS 31.6 MODULAR-EXPONENTIATION via binary square-and-multiply.
 */
int64_t mod_exp(int64_t a, int64_t b, int64_t n) {
  CLRS_ASSERT(n > 0, "n > 0");
  CLRS_ASSERT(b >= 0, "b >= 0");
  int64_t result = 1 % n;
  int64_t base = a % n;
  if (base < 0) {
    base += n;
  }
  int64_t e = b;
  while (e > 0) {
    if (e & 1) {
      result = mul_mod(result, base, n);
    }
    base = mul_mod(base, base, n);
    e >>= 1;
  }
  return result;
}

int64_t gcd_int(int64_t a, int64_t b) {
  if (a < 0) {
    a = -a;
  }
  if (b < 0) {
    b = -b;
  }
  while (b != 0) {
    int64_t t = a % b;
    a = b;
    b = t;
  }
  return a;
}

/* CLRS 31.3 EXTENDED-EUCLID: ax + by = gcd(a,b) */
int64_t extended_euclid(int64_t a, int64_t b, int64_t *x, int64_t *y) {
  if (b == 0) {
    *x = 1;
    *y = 0;
    return a;
  }
  int64_t x1, y1;
  int64_t d = extended_euclid(b, a % b, &x1, &y1);
  *x = y1;
  *y = x1 - (a / b) * y1;
  return d;
}

int64_t modular_inverse(int64_t a, int64_t m) {
  CLRS_ASSERT(m > 0, "m > 0");
  int64_t x, y;
  int64_t g = extended_euclid(a, m, &x, &y);
  if (g != 1 && g != -1) {
    return -1;
  }
  int64_t inv = x % m;
  if (inv < 0) {
    inv += m;
  }
  return inv;
}

int64_t crt_pair(int64_t a, int64_t n, int64_t b, int64_t m) {
  CLRS_ASSERT(n > 0 && m > 0, "moduli positive");
  int64_t x, y;
  int64_t g = extended_euclid(n, m, &x, &y);
  if (g != 1 && g != -1) {
    return -1;
  }
  int64_t nmod = n % m;
  if (nmod < 0) {
    nmod += m;
  }
  int64_t inv = modular_inverse(nmod, m);
  if (inv < 0) {
    return -1;
  }
  int64_t diff = (b - a) % m;
  if (diff < 0) {
    diff += m;
  }
  int64_t t = mul_mod(diff, inv, m);
  int64_t mod = n * m;
  int64_t x0 = a % mod + mul_mod(n % mod, t, mod);
  x0 %= mod;
  if (x0 < 0) {
    x0 += mod;
  }
  return x0;
}
