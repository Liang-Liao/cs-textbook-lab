/*
 * vm.h —— 栈式虚拟机接口（活动记录/调用栈，对照龙书第 7 章）
 */
#ifndef VM_H
#define VM_H

#include "ir.h"

/* 在显式运行时栈上执行 IR（lab7 的默认执行引擎）。
 * trace 非 0 时，每次 call/return 向 stderr 打印活动记录快照
 * （stdout 不受影响，照常可对拍）。
 * 返回 0；整数除零/栈溢出报告并 exit(1)。 */
int vm_run(const IrProgram *p, int trace);

#endif /* VM_H */
