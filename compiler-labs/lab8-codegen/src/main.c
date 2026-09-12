/*
 * main.c —— lab8 驱动：AST → 三地址码 → x86-64 汇编（可换回解释执行）
 *
 * 用法: ./minicc8.exe [选项] 文件.mc
 *   （缺省）检查通过 → 生成四元式 → **输出 x86-64 汇编到 stdout**
 *            链接运行两步走：
 *              ./minicc8.exe prog.mc > prog.s
 *              gcc prog.s src/rt.c -o prog.exe && ./prog.exe
 *   -ir      打印四元式清单（看回填结果！），不生成汇编
 *   -vm      栈式 VM 执行（lab7 的引擎；配合 -trace 看活动记录快照）
 *   -irvm    lab6 的 C 递归解释器执行（对照）
 *   -eval    lab5 的树遍解释器执行（对照）
 *   -ast     打印 AST（S-表达式）
 *   -symtab  打印所有作用域的符号表
 *
 * 管线形状（lab8 起"编译器"名正言顺：源码真的变成了机器码）：
 *   源码 → 词法(lab1) → 语法/AST+类型检查(lab5) → 四元式(lab6)
 *        → 执行后端三选一：x86-64(默认) / 栈式VM(lab7) / 解释器(lab6/5)
 * 同一程序四种后端输出一致——run_tests.sh 的多引擎对拍。
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
#include "codegen.h"

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
    int want_ir = 0, want_eval = 0, want_irvm = 0, want_vm = 0;
    int want_ast = 0, want_symtab = 0, want_trace = 0;
    for (int i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "-ir") == 0)     want_ir = 1;
        else if (strcmp(argv[i], "-vm") == 0)     want_vm = 1;
        else if (strcmp(argv[i], "-irvm") == 0)   want_irvm = 1;
        else if (strcmp(argv[i], "-eval") == 0)   want_eval = 1;
        else if (strcmp(argv[i], "-trace") == 0)  want_trace = 1;
        else if (strcmp(argv[i], "-ast") == 0)    want_ast = 1;
        else if (strcmp(argv[i], "-symtab") == 0) want_symtab = 1;
        else path = argv[i];
    }
    if (!path) {
        fprintf(stderr,
            "用法: %s [-ir|-vm|-irvm|-eval|-trace|-ast|-symtab] 文件.mc\n",
            argv[0]);
        return 1;
    }

    int nerr = 0;
    AstNode *prog = parse_program(read_file(path), &nerr);
    if (nerr > 0) {
        fprintf(stderr, "[minicc8] 检查出 %d 个错误，程序不会被执行\n", nerr);
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

    /* 解释类后端三兄弟：同一份 IR，三种"机器" */
    if (want_eval) { eval_run(prog);  ast_free(prog); return 0; }
    if (want_irvm) { irvm_run(irp);   ast_free(prog); return 0; }
    if (want_vm)   { vm_run(irp, want_trace); ast_free(prog); return 0; }

    codegen_emit(irp, stdout);            /* 默认：生成 x86-64 汇编 */
    ast_free(prog);
    return 0;
}
