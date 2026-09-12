/*
 * main.c —— lab4 驱动：SLR(1) 分析器
 *
 * 用法: ./minicc4.exe [选项] 文件.mc
 *   （缺省）构造 SLR(1) 表并分析文件
 *   -trace   打印每步移进/归约（状态栈+符号栈——强烈推荐！）
 *   -states  打印规范 LR(0) 项目集族
 *   -table   打印 ACTION/GOTO 表
 *   -grammar 打印文法与 FOLLOW 集
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "slr.h"
#include "grammar.h"

static char *read_file(const char *path)
{
    FILE *f = strcmp(path, "-") == 0 ? stdin : fopen(path, "rb");
    if (!f) { fprintf(stderr, "error: 无法打开 %s\n", path); exit(1); }
    if (f == stdin) {
        size_t cap = 4096, len = 0;
        char *buf = malloc(cap);
        size_t got;
        while ((got = fread(buf + len, 1, cap - len - 1, f)) > 0) {
            len += got;
            if (cap - len < 2) { cap *= 2; buf = realloc(buf, cap); }
        }
        buf[len] = '\0';
        return buf;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    size_t got = fread(buf, 1, (size_t)n, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}

/* 构造自动机与 SLR 表，打印分口径统计，并执行 slr.h 的契约：
 * shift/reduce（悬空 else）按惯例消解；reduce/reduce 是文法真歧义、
 * 无惯例可消解——直接拒绝。返回 0=可用 1=拒绝。 */
static int build_and_check(void)
{
    slr_build();
    fprintf(stderr, "[stats] states=%d  shift/reduce 冲突=%d（悬空 else,"
                    " 已按移进优先消解）  reduce/reduce 冲突=%d\n",
            slr_num_states(), slr_num_sr_conflicts(),
            slr_num_rr_conflicts());
    if (slr_num_rr_conflicts() > 0) {
        fprintf(stderr, "[minicc4] reduce/reduce 冲突无法用惯例消解，"
                        "文法不是 SLR(1)，拒绝构建分析器\n");
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    const char *path = NULL;
    int trace = 0;
    for (int i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "-trace") == 0)  trace = 1;
        else if (strcmp(argv[i], "-states") == 0) { if (build_and_check()) return 1; slr_print_states(); return 0; }
        else if (strcmp(argv[i], "-table") == 0)  { if (build_and_check()) return 1; slr_print_table(); return 0; }
        else if (strcmp(argv[i], "-grammar") == 0){ if (build_and_check()) return 1; print_grammar(); return 0; }
        else path = argv[i];
    }
    if (!path) {
        fprintf(stderr, "用法: %s [-trace|-states|-table|-grammar] 文件.mc\n",
                argv[0]);
        return 1;
    }

    if (build_and_check()) return 1;

    int r = slr_parse(read_file(path), trace);
    if (!trace) printf("SLR: %s\n", r ? "reject" : "accept");
    return r ? 1 : 0;
}
