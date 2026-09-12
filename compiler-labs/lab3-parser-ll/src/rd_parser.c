/*
 * rd_parser.c —— 递归下降语法分析器（对照龙书 2.4/4.4.1 节）
 *
 * 【形状】每个非终结符一个函数，函数体就是产生式的右部：
 *   出现终结符 -> 匹配并前进（match）
 *   出现非终结符 -> 调用它的函数
 *   有多个候选式 -> 用"向前看 1 个记号"(lookahead) 决定走哪条
 *
 * 【为什么这是现代编译器的标准姿势】GCC/Clang/TypeScript 全在用：
 * 代码即文法文档、错误恢复随便定制、性能好（函数调用即状态转移）。
 *
 * 【左结合的正确处理】文法里的 E'→+TE' 是右递归，直接翻译成递归
 * 会把 a-b-c 算成 a-(b-c)。龙书 4.3 节给出等价改写 E→T { (+|-) T }，
 * 我们用 while 循环实现"边扫边算"（acc = acc ± T），左结合天然成立。
 * lab0 运算符栈里的"等于也弹"完成的是同一件事——殊途同归。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rd_parser.h"

static Lexer lx;        /* 当前词法器 */
static Token cur;       /* 向前看记号（lookahead）——递归下降的全部"预测力" */
static int   nerr;

static void next(void) { cur = lexer_next_clean(&lx); }

static void syn_error(const char *what)
{
    nerr++;
    fprintf(stderr, "line %d, col %d: error: 语法错误: 意外的 %s\n",
            cur.line, cur.col, what);
}

/* 匹配终结符：对得上就前进，对不上就报错（简单恢复：不前进，
 * 让上层循环的 while 条件自然终止，避免死循环） */
static void match(TokenType t)
{
    if (cur.type == t) next();
    else {
        char msg[64];
        snprintf(msg, sizeof msg, "%s（期望 %s）",
                 cur.lexeme, token_name(t));
        syn_error(msg);
    }
}

/* ---------------- 值运算（int/int 保持 int，任一 float 则提升） ---------------- */
static Val arith2(TokenType op, Val a, Val b, int *ok)
{
    Val r = {0};
    *ok = 1;
    if (a.isfloat || b.isfloat) {
        double x = a.isfloat ? a.d : (double)a.i;
        double y = b.isfloat ? b.d : (double)b.i;
        r.isfloat = 1;
        switch (op) {
        case T_PLUS:    r.d = x + y; break;
        case T_MINUS:   r.d = x - y; break;
        case T_STAR:    r.d = x * y; break;
        case T_SLASH:   r.d = x / y; break;
        default: *ok = 0; return r;  /* 含浮点无 % */
        }
    } else {
        if (op == T_SLASH && b.i == 0) { *ok = 0; return r; }
        if (op == T_PERCENT && b.i == 0) { *ok = 0; return r; }
        switch (op) {
        case T_PLUS:    r.i = a.i + b.i; break;
        case T_MINUS:   r.i = a.i - b.i; break;
        case T_STAR:    r.i = a.i * b.i; break;
        case T_SLASH:   r.i = a.i / b.i; break;   /* C 语义：向零截断 */
        case T_PERCENT: r.i = a.i % b.i; break;
        default: break;
        }
    }
    return r;
}

static Val compare(TokenType op, Val a, Val b)
{
    double x = a.isfloat ? a.d : (double)a.i;
    double y = b.isfloat ? b.d : (double)b.i;
    Val r = {0, 0, 0};
    switch (op) {
    case T_LT: r.i = x <  y; break;
    case T_LE: r.i = x <= y; break;
    case T_GT: r.i = x >  y; break;
    case T_GE: r.i = x >= y; break;
    case T_EQ: r.i = x == y; break;
    case T_NEQ: r.i = x != y; break;
    default: break;
    }
    return r;
}

static void check(int ok, const char *what)
{
    if (!ok) {
        nerr++;
        fprintf(stderr, "line %d, col %d: error: %s\n",
                cur.line, cur.col, what);
    }
}

/* ---------------- 每个非终结符一个函数，优先级层层下降 ----------------
 * 调用链越深优先级越高：expr → equality → rel → add → term → factor */

static Val parse_expr(void);

static Val parse_factor(void)
{
    if (cur.type == T_LPAREN) {
        next();                       /* 吃掉 '(' */
        Val v = parse_expr();
        match(T_RPAREN);
        return v;
    }
    if (cur.type == T_MINUS) {        /* 一元负号: factor → - factor */
        next();
        Val v = parse_factor();
        if (v.isfloat) v.d = -v.d;
        else           v.i = -v.i;
        return v;
    }
    if (cur.type == T_INT_LIT) {
        Val v = {0, cur.ival, 0};
        next();
        return v;
    }
    if (cur.type == T_FLOAT_LIT) {
        Val v = {1, 0, cur.dval};
        next();
        return v;
    }
    syn_error(cur.lexeme);
    next(); /* 错误恢复：吞掉这个记号，返回 0 让分析继续 */
    Val v = {0, 0, 0};
    return v;
}

/* term → factor { (*|/|%) factor }   —— 乘除模层（左结合：循环累计） */
static Val parse_term(void)
{
    Val acc = parse_factor();
    while (cur.type == T_STAR || cur.type == T_SLASH ||
           cur.type == T_PERCENT) {
        TokenType op = cur.type;
        next();
        Val r = parse_factor();
        int wasfloat = acc.isfloat || r.isfloat;
        int ok;
        acc = arith2(op, acc, r, &ok);
        /* 失败原因三分：整型 / 或 % 的零除数报"除数为零"；浮点不支持
         * %（与除数是否为零无关）才报"浮点数不能取模"。旧文案只看
         * 运算符，整型的 10 % 0 被误报成后者。 */
        check(ok, (op == T_SLASH || (op == T_PERCENT && !wasfloat))
                  ? "除数为零" : "浮点数不能取模");
    }
    return acc;
}

/* add → term { (+|-) term }          —— 加减层 */
static Val parse_add(void)
{
    Val acc = parse_term();
    while (cur.type == T_PLUS || cur.type == T_MINUS) {
        TokenType op = cur.type;
        next();
        int ok;
        Val r = parse_term();
        acc = arith2(op, acc, r, &ok);
        (void)ok;
    }
    return acc;
}

/* rel → add { (<|<=|>|>=) add }      —— 关系层（结果是 0/1） */
static Val parse_rel(void)
{
    Val acc = parse_add();
    while (cur.type == T_LT || cur.type == T_LE ||
           cur.type == T_GT || cur.type == T_GE) {
        TokenType op = cur.type;
        next();
        Val r = parse_add();
        acc = compare(op, acc, r);
    }
    return acc;
}

/* equality → rel { (==|!=) rel }     —— 相等层 */
static Val parse_equality(void)
{
    Val acc = parse_rel();
    while (cur.type == T_EQ || cur.type == T_NEQ) {
        TokenType op = cur.type;
        next();
        Val r = parse_rel();
        acc = compare(op, acc, r);
    }
    return acc;
}

static Val parse_expr(void) { return parse_equality(); }

/* stmt → print expr ; | expr ; */
static void parse_stmt(void)
{
    if (cur.type == T_KW_PRINT) {
        next();
        Val v = parse_expr();
        match(T_SEMI);
        if (v.isfloat) printf("%g\n", v.d);
        else           printf("%ld\n", v.i);
    } else {
        parse_expr();
        match(T_SEMI);
    }
}

int rd_parse(const char *src)
{
    lexer_init(&lx, src);
    nerr = 0;
    next();

    while (cur.type != T_EOF)      /* stmt_list → stmt stmt_list | ε */
        parse_stmt();

    return nerr ? 1 : 0;
}
