#include "test.h"

void test_init(TestSuite *suite) {
  suite->total = 0;
  suite->failed = 0;
}

int test_check(TestSuite *suite, int ok, const char *file, int line,
               const char *expr) {
  suite->total++;
  if (ok) {
    return 1;
  }
  suite->failed++;
  fprintf(stderr, "FAIL %s:%d  %s\n", file, line, expr);
  return 0;
}

int test_report(const TestSuite *suite, const char *name) {
  int failed = suite->failed;
  int total = suite->total;
  if (failed == 0) {
    printf("[PASS] %s  (%d checks)\n", name, total);
    return 0;
  }
  printf("[FAIL] %s  (%d/%d checks failed)\n", name, failed, total);
  return 1;
}
