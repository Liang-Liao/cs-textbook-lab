/*
 * token.h —— 记号(token)的定义（lab1，对照龙书第 3 章 3.1/3.4 节）
 *
 * 词法分析器的输出是"记号流"。每个记号有三要素：
 *   1. 类别(type)   —— 属于哪一类词法单元，如"标识符""加号"
 *   2. 词素(lexeme) —— 源代码中匹配到的原始字符串，如 "radius"
 *   3. 位置(line,col) —— 用于报错定位（现代编译器还用它做"点一下
 *                      跳转到定义"等功能）
 *
 * 另外字面量记号要带走"值"（ival/dval），这是语法制导翻译里
 * 最典型的综合属性（龙书第 5 章）——词法阶段就算出来的值，
 * 一路传给后面的阶段使用。
 */
#ifndef TOKEN_H
#define TOKEN_H

/* 记号类别。命名约定：T_KW_* 关键字、T_*_LIT 字面量、T_* 运算符标点 */
typedef enum {
    T_EOF = 0,   /* 输入结束（一个"永远存在"的哨兵记号，语法分析依赖它） */

    /* ---- 关键字（保留字）: lab3/lab4 的文法要用到的都先备好 ---- */
    T_KW_INT, T_KW_FLOAT, T_KW_VOID,
    T_KW_IF, T_KW_ELSE, T_KW_WHILE, T_KW_RETURN,
    T_KW_BREAK, T_KW_CONTINUE, T_KW_PRINT,

    /* ---- 字面量 ---- */
    T_INT_LIT,   /* 123        -> ival  */
    T_FLOAT_LIT, /* 3.14       -> dval  */

    /* ---- 标识符 ---- */
    T_IDENT,     /* radius, _tmp1, ... */

    /* ---- 运算符与标点（双字符的先写，便于和单字符对照） ---- */
    T_PLUS,        /* +  */
    T_MINUS,       /* -  */
    T_STAR,        /* *  */
    T_SLASH,       /* /  */
    T_PERCENT,     /* %  */
    T_ASSIGN,      /* =  */
    T_EQ,          /* == */
    T_NEQ,         /* != */
    T_LT,          /* <  */
    T_LE,          /* <= */
    T_GT,          /* >  */
    T_GE,          /* >= */
    T_NOT,         /* !  */
    T_ANDAND,      /* && */
    T_OROR,        /* || */
    T_LPAREN,      /* (  */
    T_RPAREN,      /* )  */
    T_LBRACE,      /* {  */
    T_RBRACE,      /* }  */
    T_LBRACKET,    /* [  */
    T_RBRACKET,    /* ]  */
    T_SEMI,        /* ;  */
    T_COMMA,       /* ,  */

    /* ---- 错误记号：词法错误恢复用（见 lexer.c 尾部说明） ---- */
    T_ERROR
} TokenType;

/* 记号结构体 */
typedef struct {
    TokenType   type;    /* 类别 */
    const char *lexeme;  /* 词素（指向符号表里的驻留字符串，不释放） */
    long        ival;    /* T_INT_LIT  的整数值 */
    double      dval;    /* T_FLOAT_LIT的浮点值 */
    int         line;    /* 行号（1 起） */
    int         col;     /* 列号（1 起，按字节计——见 lexer.c 里的简化说明） */
} Token;

/* 类别 -> 可打印名字（如 "T_IDENT"），用于打印 token 流/报错 */
const char *token_name(TokenType t);

#endif /* TOKEN_H */
