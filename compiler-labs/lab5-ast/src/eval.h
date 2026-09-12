/*
 * eval.h —— AST 树遍解释执行接口
 */
#ifndef EVAL_H
#define EVAL_H

#include "ast.h"

/* 解释执行整棵程序（print 输出到 stdout）。
 * 返回 0；整数除零/取模零是唯一的运行时错误，直接报告并 exit(1)
 * ——类型错误等"编译期"能拦的都已在 parse 阶段拦下（编译通过才执行）。 */
int eval_run(const AstNode *prog);

#endif /* EVAL_H */
