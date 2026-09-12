/*
 * eval.h —— AST 树遍解释执行接口
 */
#ifndef EVAL_H
#define EVAL_H

#include "ast.h"

/* 树遍解释执行整棵程序（lab5 的执行方式，lab6 作为 IR 执行的"参考实现"，
 * 两种执行方式的输出必须逐字节一致——见 run_tests.sh 的双实现对拍）。
 * 返回 0；整数除零/取模零直接报告并 exit(1)。 */
int eval_run(const AstNode *prog);

#endif /* EVAL_H */
