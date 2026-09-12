/*
 * main.c —— lab5 驱动：语法制导翻译 → AST → 检查 → 执行
 *
 * 用法: ./minicc5.exe [选项] 文件.mc
 *   （缺省）分析 + 类型检查，全部通过后解释执行（print 输出结果）
 *   -ast     检查通过后打印 AST（S-表达式），不执行
 *   -symtab  检查通过后打印所有作用域的符号表，不执行
 *
 * 有任何语法/语义错误时不执行——lab3 是"边分析边执行、错了继续"，
 * lab5 有了 AST 这棵完整的树，自然升级为"先检查、后执行"两趟：
 * 这正是"编译器"与"解释器"的分界线，lab6 起两趟之间还要再插入
 * 生成中间代码的一趟。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "symtab.h"
#include "eval.h"

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

int main(int argc, char **argv)
{
    const char *path = NULL;
    int want_ast = 0, want_symtab = 0;
    for (int i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "-ast") == 0)    want_ast = 1;
        else if (strcmp(argv[i], "-symtab") == 0) want_symtab = 1;
        else path = argv[i];
    }
    if (!path) {
        fprintf(stderr, "用法: %s [-ast|-symtab] 文件.mc\n", argv[0]);
        return 1;
    }

    int nerr = 0;
    AstNode *prog = parse_program(read_file(path), &nerr);
    if (nerr > 0) {
        fprintf(stderr, "[minicc5] 检查出 %d 个错误，程序不会被执行\n", nerr);
        ast_free(prog);
        return 1;
    }

    if (want_ast)    { ast_dump(prog);    ast_free(prog); return 0; }
    if (want_symtab) { symtab_dump();     ast_free(prog); return 0; }

    eval_run(prog);          /* 树遍解释执行：print 的结果都在这里 */
    ast_free(prog);
    return 0;
}
