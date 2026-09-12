/*
 * opt.h —— 四元式 IR 优化器接口（lab9 主角；对照龙书 8.4~8.5、9.1~9.2 节）
 */
#ifndef OPT_H
#define OPT_H

#include "ir.h"

/* 原地优化 IR：常量折叠/传播、局部公共子表达式消除、基于活跃变量
 * 分析的死代码删除、窥孔清理，lab10/11 的循环级 pass（外提、依赖
 * 门禁展开、完美嵌套交换），以及 lab12 的过程间分析（IPCP 常量
 * 代入、函数内联；mod/ref 集合供各 pass 精确化 CALL 保守规则）。
 * 外层迭代至不动点。
 * 返回是否修改了 IR；统计摘要打到 stderr（stdout 不受影响）。 */
int opt_run(IrProgram *p);

/* 打印每段的控制流图：基本块区间、后继边、支配集合、自然循环清单
 * （-O 配合 -ir 使用；独立教学观测开关 -cfg 也走这里）。 */
void cfg_dump(const IrProgram *p);

/* lab12 观测点：调用图转储（-cg）。结点 = 函数段，边 = CALL 四元式；
 * 叶子与递归环就地标注。作用于**传入时的 IR 形态**（main.c 固定在
 * gen 之后、-O 之前调用——看的是源程序本来的调用结构），打 stdout
 * （对齐 -cfg 的结构转储惯例），纯 ASCII 可对拍。 */
void cg_dump(const IrProgram *p);

/* lab11 观测点：依赖分析与迭代空间可视化。两者都作用于**传入时的
 * IR 形态**（main.c 固定在 gen 之后、-O 之前调用——分析源程序本来
 * 的循环结构，而不是被 LICM/展开改写后的形状），结果打 **stderr**
 * （与 [opt] 统计同侧；stdout 留给程序自身输出），纯 ASCII 可对拍。
 * deps_dump 输出同环访存语句对的距离向量与类别；space_dump 对完美
 * 二重嵌套画迭代空间网格 + 依赖箭头样例。 */
void deps_dump(const IrProgram *p);
void space_dump(const IrProgram *p);

#endif /* OPT_H */
