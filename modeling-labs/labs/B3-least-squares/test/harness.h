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

/*
 * CLI:
 *   test                 全量：所有算法所有场景
 *   test --list          列出 suite/场景
 *   test linalg          只跑某个算法的全部场景
 *   test linalg/lu_xx    只跑某一场景（也支持 linalg:lu_xx）
 *   test a b c           多个算法/场景
 * 退出码：所选用例全 PASS 则 0
 */
int test_run_main(const char *lab_id, const test_case *cases, int ncases,
                  int argc, char **argv);

#endif
