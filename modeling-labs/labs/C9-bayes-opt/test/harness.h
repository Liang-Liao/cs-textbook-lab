#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <stdio.h>

typedef int (*test_fn)(void);

typedef struct {
    const char *suite;
    const char *name;
    const char *doc;
    test_fn fn;
} test_case;

void test_record(int ok, const char *suite, const char *name, const char *detail);
int test_pass_count(void);
int test_fail_count(void);
void test_reset_counters(void);
void test_ensure_results_dir(void);

int test_run_main(const char *lab_id, const test_case *cases, int ncases,
                  int argc, char **argv);

#endif
