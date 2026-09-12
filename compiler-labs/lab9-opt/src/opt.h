/*
 * opt.h —— 四元式 IR 优化器接口（lab9 主角；对照龙书 8.4~8.5、9.1~9.2 节）
 */
#ifndef OPT_H
#define OPT_H

#include "ir.h"

/* 原地优化 IR：常量折叠/传播、局部公共子表达式消除、基于活跃变量
 * 分析的死代码删除、窥孔清理。外层迭代至不动点。
 * 返回是否修改了 IR；统计摘要打到 stderr（stdout 不受影响）。 */
int opt_run(IrProgram *p);

#endif /* OPT_H */
