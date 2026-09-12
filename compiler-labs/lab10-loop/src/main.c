/*
 * main.c —— lab10 驱动：AST → 四元式 →【优化】→ 后端（四选一）
 *
 * 用法: ./minicc10.exe [-O] [选项] 文件.mc
 *   （缺省）生成 x86-64 汇编到 stdout；链接运行：
 *              ./minicc10.exe prog.mc > prog.s
 *              gcc prog.s src/rt.c -o prog.exe && ./prog.exe
 *   -O       在 gen 之后、后端之前跑优化器（常量折叠/传播、CSE、
 *            活跃变量死代码删除、窥孔），统计摘要打 stderr。
 *            不带 -O 行为与 lab8 一致——同一程序两种形态输出必须
 *            一致，这正是对拍的意义。
 *   -cfg     控制流图转储：基本块/后继/支配集/自然循环（lab10 观测点）
 *   -ir      打印四元式清单；配合 -O 看到的是**优化后**的 IR——
 *            与不带 -O 的 dump 一对照，每类优化的效果肉眼可见
 *   -vm      栈式 VM 执行（lab7 引擎，可配 -trace 看活动记录）
 *   -irvm    lab6 的 C 递归解释器执行
 *   -eval    lab5 的树遍解释器执行
 *   -ast     打印 AST；-symtab 打印符号表
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
#include "opt.h"

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
    int want_cfg = 0;
    int want_ast = 0, want_symtab = 0, want_trace = 0, want_O = 0;
    for (int i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "-ir") == 0)     want_ir = 1;
        else if (strcmp(argv[i], "-cfg") == 0)    want_cfg = 1;
        else if (strcmp(argv[i], "-O") == 0)      want_O = 1;
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
            "用法: %s [-O] [-ir|-cfg|-vm|-irvm|-eval|-trace|-ast|-symtab] 文件.mc\n",
            argv[0]);
        return 1;
    }

    int nerr = 0;
    AstNode *prog = parse_program(read_file(path), &nerr);
    if (nerr > 0) {
        fprintf(stderr, "[minicc10] 检查出 %d 个错误，程序不会被执行\n", nerr);
        ast_free(prog);
        return 1;
    }

    if (want_ast)    { ast_dump(prog);   ast_free(prog); return 0; }
    if (want_symtab) { symtab_dump();    ast_free(prog); return 0; }

    IrProgram *irp = gen_program(prog);   /* AST → 四元式（回填在这完成） */
    if (want_O)
        opt_run(irp);                     /* lab9 折叠/CSE/DCE + lab10 循环优化 */
    if (want_cfg) {                       /* 控制流图转储（-O 后的最终形状） */
        cfg_dump(irp);
        ast_free(prog);
        return 0;
    }
    if (want_ir) {                        /* 只打印（-O 时是优化后的形态） */
        ir_print(irp);
        ast_free(prog);
        return 0;
    }

    /* 解释类后端三兄弟：同一份（可能优化过的）IR，三种"机器" */
    if (want_eval) { eval_run(prog);  ast_free(prog); return 0; }
    if (want_irvm) { irvm_run(irp);   ast_free(prog); return 0; }
    if (want_vm)   { vm_run(irp, want_trace); ast_free(prog); return 0; }

    codegen_emit(irp, stdout);            /* 默认：生成 x86-64 汇编 */
    ast_free(prog);
    return 0;
}
