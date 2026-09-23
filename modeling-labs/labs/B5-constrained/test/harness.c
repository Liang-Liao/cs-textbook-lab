#include "harness.h"

#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

static int g_pass = 0, g_fail = 0;

void test_record(int ok, const char *suite, const char *name, const char *detail)
{
    if (ok) {
        ++g_pass;
        printf("PASS  [%s] %s — %s\n", suite, name, detail ? detail : "");
    } else {
        ++g_fail;
        printf("FAIL  [%s] %s — %s\n", suite, name, detail ? detail : "");
    }
}

int test_pass_count(void) { return g_pass; }
int test_fail_count(void) { return g_fail; }
void test_reset_counters(void) { g_pass = g_fail = 0; }

void test_ensure_results_dir(void)
{
#ifdef _WIN32
    _mkdir("results");
#else
    mkdir("results", 0777);
#endif
}

static void print_usage(const char *prog)
{
    printf("Usage: %s [--list|--help] [suite|suite/name ...]\n", prog);
    printf("  (no args)     full functional test: all suites\n");
    printf("  suite         all cases of one algorithm\n");
    printf("  suite/name    single case (also suite:name)\n");
    printf("  --list        list suites and cases\n");
}

static int case_matches(const test_case *c, int argc, char **argv)
{
    int i;
    if (argc <= 0) return 1;
    for (i = 0; i < argc; ++i) {
        const char *a = argv[i];
        const char *sep = strchr(a, '/');
        char suite_buf[128];
        size_t slen;
        if (strcmp(a, c->suite) == 0) return 1;
        if (!sep) sep = strchr(a, ':');
        if (!sep) continue;
        slen = (size_t)(sep - a);
        if (slen == 0 || slen >= sizeof suite_buf) continue;
        memcpy(suite_buf, a, slen);
        suite_buf[slen] = '\0';
        if (strcmp(suite_buf, c->suite) == 0 && strcmp(sep + 1, c->name) == 0)
            return 1;
    }
    return 0;
}

static int has_flag(int argc, char **argv, const char *long_f, const char *short_f)
{
    int i;
    for (i = 0; i < argc; ++i) {
        if (strcmp(argv[i], long_f) == 0) return 1;
        if (short_f && strcmp(argv[i], short_f) == 0) return 1;
    }
    return 0;
}

int test_run_main(const char *lab_id, const test_case *cases, int ncases,
                  int argc, char **argv)
{
    char *filters[64];
    int nf = 0, i, selected = 0;
    const char *prog = (argc > 0 && argv[0]) ? argv[0] : "test";

    test_reset_counters();

    if (has_flag(argc, argv, "--help", "-h")) {
        print_usage(prog);
        return 0;
    }
    if (has_flag(argc, argv, "--list", "-l")) {
        printf("lab %s — suites:\n", lab_id);
        for (i = 0; i < ncases; ++i)
            printf("  %s/%s\n      %s\n", cases[i].suite, cases[i].name,
                   cases[i].doc ? cases[i].doc : "");
        return 0;
    }

    for (i = 1; i < argc && nf < 64; ++i) {
        if (argv[i][0] == '-') continue;
        filters[nf++] = argv[i];
    }

    printf("test %s\n", lab_id);
    if (nf == 0)
        printf("mode: FULL\n");
    else {
        printf("mode: SELECT");
        for (i = 0; i < nf; ++i) printf(" %s", filters[i]);
        printf("\n");
    }

    for (i = 0; i < ncases; ++i) {
        if (!case_matches(&cases[i], nf, filters)) continue;
        ++selected;
        printf("== %s/%s: %s\n", cases[i].suite, cases[i].name,
               cases[i].doc ? cases[i].doc : "");
        cases[i].fn();
    }

    if (selected == 0) {
        printf("no matching test cases\n");
        print_usage(prog);
        return 1;
    }
    printf("\nSummary: %d PASS, %d FAIL (%d cases)\n",
           test_pass_count(), test_fail_count(), selected);
    return test_fail_count() ? 1 : 0;
}
