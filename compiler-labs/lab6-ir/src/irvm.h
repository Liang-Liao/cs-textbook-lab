/*
 * irvm.h —— 四元式解释执行接口
 */
#ifndef IRVM_H
#define IRVM_H

#include "ir.h"

/* 解释执行 IR（lab6 的默认执行引擎）。
 * 返回 0；整数除零/取模零报告并 exit(1)。lab7 把本文件的"C 递归当
 * 调用栈"显式化成活动记录 + 栈式虚拟机。 */
int irvm_run(const IrProgram *p);

#endif /* IRVM_H */
