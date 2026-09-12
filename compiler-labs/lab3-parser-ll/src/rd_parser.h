/*
 * rd_parser.h —— 手写递归下降语法分析器 + 直接求值（lab3 Part A）
 */
#ifndef RD_PARSER_H
#define RD_PARSER_H

#include "lexer.h"

/* MiniC v2 的值：int 或 float 的运行时表示（lab5 之前还没有变量与类型
 * 系统，先用"标签联合"最简实现；这也是动态类型语言值的经典表示） */
typedef struct {
    int    isfloat;
    long   i;
    double d;
} Val;

/* 分析整个源文件。每个 print 语句输出一行结果；返回 0=接受 1=有语法错误 */
int rd_parse(const char *src);

#endif /* RD_PARSER_H */
