#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <stdio.h>

/* 业务算法功能测试：suite=算法组，name=场景用例 */

typedef int (*test_fn)(void);

typedef struct {
    const char *suite; /* 算法名，如 "linalg" */
    const char *name;  /* 场景名，如 "lu_well_conditioned" */
    const char *doc;   /* 一句话说明本场景在测什么 */
    test_fn fn;        /* 返回 1=PASS, 0=FAIL */
} test_case;

void test_record(int ok, const char *suite, const char *name, const char *detail);
int test_pass_count(void);
int test_fail_count(void);
void test_reset_counters(void);
void test_ensure_results_dir(void);

int test_run_main(const char *lab_id, const test_case *cases, int ncases,
                  int argc, char **argv);

#endif
