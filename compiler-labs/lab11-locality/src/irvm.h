/*
 * irvm.h —— 四元式解释执行接口（lab7 里降级为 -irvm 对照引擎）
 */
#ifndef IRVM_H
#define IRVM_H

#include "ir.h"

/* 解释执行 IR（lab6 的默认执行引擎；lab7 起由 vm.c 的栈式 VM 接棒，
 * 本文件保留作对照——它把调用栈交给 C 递归，VM 则显式管理活动记录。
 * 返回 0；整数除零/取模零报告并 exit(1)。 */
int irvm_run(const IrProgram *p);

#endif /* IRVM_H */
