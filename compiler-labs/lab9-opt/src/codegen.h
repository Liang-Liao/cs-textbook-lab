/*
 * codegen.h —— 四元式 → x86-64 汇编（AT&T 语法、Windows x64 调用约定）
 */
#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>
#include "ir.h"

/* 把 IR 翻译成一份完整汇编文件（.text + 全局 .comm + 浮点字面量池），
 * 写到 out。产物用 "gcc prog.s src/rt.c" 汇编链接成本机可执行文件。
 * 返回 0；IR 违反 gen 不变式时报告内部错误并 exit(2)。 */
int codegen_emit(const IrProgram *p, FILE *out);

#endif /* CODEGEN_H */
