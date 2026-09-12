/*
 * main.c —— lab6 驱动：AST → 三地址码 → IR 解释执行
 *
 * 用法: ./minicc6.exe [选项] 文件.mc
 *   （缺省）检查通过 → 生成四元式 → IR 解释执行（print 输出结果）
 *   -ir      检查通过后打印四元式清单（看回填结果！），不执行
 *   -eval    用 lab5 的树遍解释器执行（与 IR 执行对照）
 *   -ast     打印 AST（S-表达式），不执行
 *   -symtab  打印所有作用域的符号表（函数签名也可见），不执行
 *
 * 管线形状（lab6 起"编译器"的样子齐了）：
 *   源码 → 词法(lab1) → 语法/AST+类型检查(lab5) → 四元式(lab6) → 执行
 * lab7 把"执行"换成显式栈帧的虚拟机，lab8 再换成真机器码。
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
    int want_ir = 0, want_eval = 0, want_ast = 0, want_symtab = 0;
    for (int i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "-ir") == 0)     want_ir = 1;
        else if (strcmp(argv[i], "-eval") == 0)   want_eval = 1;
        else if (strcmp(argv[i], "-ast") == 0)    want_ast = 1;
        else if (strcmp(argv[i], "-symtab") == 0) want_symtab = 1;
        else path = argv[i];
    }
    if (!path) {
        fprintf(stderr, "用法: %s [-ir|-eval|-ast|-symtab] 文件.mc\n", argv[0]);
        return 1;
    }

    int nerr = 0;
    AstNode *prog = parse_program(read_file(path), &nerr);
    if (nerr > 0) {
        fprintf(stderr, "[minicc6] 检查出 %d 个错误，程序不会被执行\n", nerr);
        ast_free(prog);
        return 1;
    }

    if (want_ast)    { ast_dump(prog);   ast_free(prog); return 0; }
    if (want_symtab) { symtab_dump();    ast_free(prog); return 0; }

    if (want_eval) {          /* 参考实现：树遍解释（lab5 方式） */
        eval_run(prog);
        ast_free(prog);
        return 0;
    }

    IrProgram *irp = gen_program(prog);   /* AST → 四元式（回填在这完成） */
    if (want_ir) {            /* 只打印不执行 */
        ir_print(irp);
        ast_free(prog);
        return 0;
    }

    irvm_run(irp);            /* 默认：IR 解释执行 */
    ast_free(prog);
    return 0;
}
