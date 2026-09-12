/*
 * main.c —— lab3 驱动：同一个文件，跑两套语法分析器对拍
 *
 * 用法: ./minicc3.exe [选项] 文件.mc
 *   （缺省）先跑递归下降（求值，print 有输出），再跑 LL(1)（识别）
 *   -ll      只跑 LL(1)
 *   -rd      只跑递归下降
 *   -trace   LL(1) 打印每步分析栈/输入/动作（强烈推荐看一遍！）
 *   -ff      打印 FIRST/FOLLOW 后退出
 *   -table   打印 LL(1) 分析表后退出
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "rd_parser.h"
#include "ll_parser.h"
#include "grammar.h"

static char *read_file(const char *path)
{
    FILE *f = strcmp(path, "-") == 0 ? stdin : fopen(path, "rb");
    if (!f) { fprintf(stderr, "error: 无法打开 %s\n", path); exit(1); }
    if (f == stdin) {
        /* stdin 无法 fseek，逐块读 */
        size_t cap = 4096, len = 0;
        char *buf = malloc(cap);
        size_t got;
        while ((got = fread(buf + len, 1, cap - len - 1, f)) > 0) {
            len += got;
            if (cap - len < 2) {
                cap *= 2;
                buf = realloc(buf, cap);
            }
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

int main(int argc, char **argv)
{
    const char *path = NULL;
    int mode_rd = 1, mode_ll = 1, trace = 0;
    for (int i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "-rd") == 0)   { mode_rd = 1; mode_ll = 0; }
        else if (strcmp(argv[i], "-ll") == 0)   { mode_rd = 0; mode_ll = 1; }
        else if (strcmp(argv[i], "-trace") == 0) trace = 1;
        else if (strcmp(argv[i], "-ff") == 0) {
            compute_first_follow();
            print_first_follow();
            return 0;
        }
        else if (strcmp(argv[i], "-table") == 0) {
            compute_first_follow();
            build_ll_table();
            print_ll_table();
            return ll_conflicts() ? 1 : 0;
        }
        else path = argv[i];
    }
    if (!path) {
        fprintf(stderr, "用法: %s [-rd|-ll|-trace|-ff|-table] 文件.mc\n",
                argv[0]);
        return 1;
    }

    /* 两套分析器共享同一套"文法机制"：先算 FIRST/FOLLOW 和分析表 */
    compute_first_follow();
    build_ll_table();
    if (ll_conflicts()) {
        fprintf(stderr, "error: 文法存在 LL(1) 冲突，无法继续\n");
        return 1;
    }

    char *src = read_file(path);
    int rc = 0;

    if (mode_rd) rc |= rd_parse(src);
    if (mode_ll) {
        int r = ll_parse(src, trace);
        if (!trace) printf("LL: %s\n", r ? "reject" : "accept");
        rc |= r;
    }
    return rc ? 1 : 0;
}
