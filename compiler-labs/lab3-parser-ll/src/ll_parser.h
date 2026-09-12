/*
 * ll_parser.h —— LL(1) 表驱动预测分析器：显式栈 + panic 错误恢复
 *                （lab3 Part B，对照龙书 4.4.2~4.4.3 节）
 */
#ifndef LL_PARSER_H
#define LL_PARSER_H

/* 用 grammar.c 算出的 FIRST/FOLLOW/分析表跑一遍输入（只做识别，
 * 不求值——表驱动做语法制导求值要等 lab5 的 AST）。
 * trace=1 时打印每一步：栈内容 + 剩余输入 + 动作。
 * 返回 0=接受 1=有语法错误。 */
int ll_parse(const char *src, int trace);

#endif /* LL_PARSER_H */
