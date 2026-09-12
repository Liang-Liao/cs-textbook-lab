/*
 * lexer.h —— 手写词法分析器接口（lab1，对照龙书第 3 章）
 */
#ifndef LEXER_H
#define LEXER_H

#include "token.h"

/* 词法分析器状态：整个源文件一次性读入内存（"现代做法"，
 * 龙书 3.2 节的双缓冲区是慢速 I/O 时代的技术，已被淘汰——
 * 见 README 的"龙书 vs 现代"）。 */
typedef struct {
    const char *src; /* 源文件内容（NUL 结尾） */
    int         pos; /* 当前读到的下标（游标） */
    int         line, col; /* 游标所在的行列（1 起） */
} Lexer;

void  lexer_init(Lexer *lx, const char *src);
Token lexer_next(Lexer *lx); /* 扫描下一个记号；到达结尾后持续返回 T_EOF */

/* 到目前为止是否出现过词法错误（决定退出码） */
int   lexer_had_error(void);

#endif /* LEXER_H */
