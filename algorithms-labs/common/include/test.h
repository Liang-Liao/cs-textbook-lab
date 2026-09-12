#ifndef CLRS_TEST_H
#define CLRS_TEST_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int total;
  int failed;
} TestSuite;

void test_init(TestSuite *suite);

int test_check(TestSuite *suite, int ok, const char *file, int line,
               const char *expr);

#define ASSERT_TRUE(suite, cond)                                               \
  test_check((suite), (cond) ? 1 : 0, __FILE__, __LINE__, #cond)

#define ASSERT_FALSE(suite, cond)                                              \
  test_check((suite), (cond) ? 0 : 1, __FILE__, __LINE__, "!(" #cond ")")

#define ASSERT_EQ_INT(suite, actual, expected)                                 \
  do {                                                                         \
    long long _a = (long long)(actual);                                        \
    long long _e = (long long)(expected);                                      \
    int _ok = (_a == _e);                                                      \
    if (!_ok) {                                                                \
      fprintf(stderr, "  expected %lld, got %lld\n", _e, _a);                  \
    }                                                                          \
    test_check((suite), _ok, __FILE__, __LINE__, #actual " == " #expected);    \
  } while (0)

/* Returns process exit code: 0 if all passed, 1 otherwise. */
int test_report(const TestSuite *suite, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_TEST_H */
