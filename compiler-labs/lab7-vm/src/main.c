/*
 * main.c —— lab7 驱动：AST → 三地址码 → 栈式虚拟机执行
 *
 * 用法: ./minicc7.exe [选项] 文件.mc
 *   （缺省）检查通过 → 生成四元式 → 栈式 VM 执行（print 输出结果）
 *   -ir      检查通过后打印四元式清单（看回填结果！），不执行
 *   -irvm    用 lab6 的 C 递归解释器执行（对照引擎：C 调用栈兼任 MiniC 调用栈）
 *   -eval    用 lab5 的树遍解释器执行（对照引擎：完全不经过 IR）
 *   -trace   栈式 VM 每次调用/返回时向 stderr 打印活动记录快照（看栈！）
 *   -ast     打印 AST（S-表达式），不执行
 *   -symtab  打印所有作用域的符号表（函数签名也可见），不执行
 *
 * 管线形状（lab7 起"后端"有了第一台真机器的样子）：
 *   源码 → 词法(lab1) → 语法/AST+类型检查(lab5) → 四元式(lab6) → 栈式VM(lab7)
 *                                                                └→ x86-64(lab8)
 * 三台引擎跑同一份 IR/AST：vm(默认)/irvm/eval 输出必须一致——run_tests.sh
 * 的三重对拍。lab6 抓出的三个 bug 全靠这类对拍，lab7 沿用同一保险。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "symtab.h"
#include "eval.h"
#include "gen.h"
#include "ir.h"
#include "irvm.h"
#include "vm.h"

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
    int want_ir = 0, want_eval = 0, want_irvm = 0;
    int want_ast = 0, want_symtab = 0, want_trace = 0;
    for (int i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "-ir") == 0)     want_ir = 1;
        else if (strcmp(argv[i], "-irvm") == 0)   want_irvm = 1;
        else if (strcmp(argv[i], "-eval") == 0)   want_eval = 1;
        else if (strcmp(argv[i], "-trace") == 0)  want_trace = 1;
        else if (strcmp(argv[i], "-ast") == 0)    want_ast = 1;
        else if (strcmp(argv[i], "-symtab") == 0) want_symtab = 1;
        else path = argv[i];
    }
    if (!path) {
        fprintf(stderr,
            "用法: %s [-ir|-irvm|-eval|-trace|-ast|-symtab] 文件.mc\n",
            argv[0]);
        return 1;
    }

    int nerr = 0;
    AstNode *prog = parse_program(read_file(path), &nerr);
    if (nerr > 0) {
        fprintf(stderr, "[minicc7] 检查出 %d 个错误，程序不会被执行\n", nerr);
        ast_free(prog);
        return 1;
    }

    if (want_ast)    { ast_dump(prog);   ast_free(prog); return 0; }
    if (want_symtab) { symtab_dump();    ast_free(prog); return 0; }

    IrProgram *irp = gen_program(prog);   /* AST → 四元式（回填在这完成） */
    if (want_ir) {            /* 只打印不执行 */
        ir_print(irp);
        ast_free(prog);
        return 0;
    }

    if (want_eval) {          /* 参考实现一：树遍解释（lab5 方式） */
        eval_run(prog);
        ast_free(prog);
        return 0;
    }
    if (want_irvm) {          /* 参考实现二：IR 解释、C 递归当调用栈 */
        irvm_run(irp);
        ast_free(prog);
        return 0;
    }

    vm_run(irp, want_trace);  /* 默认：显式活动记录的栈式 VM */
    ast_free(prog);
    return 0;
}
