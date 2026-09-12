/*
 * parser.h —— 递归下降 + 语法制导翻译接口
 */
#ifndef PARSER_H
#define PARSER_H

#include "ast.h"

/* 分析整个源文件：成功返回 AST 根（A_PROG），语法/语义错误数写入 *nerr。
 * 无论有无错误都尽量构造出（部分的）AST——错误恢复让分析"跑完再算总账"。 */
AstNode *parse_program(const char *src, int *nerr);

#endif /* PARSER_H */
