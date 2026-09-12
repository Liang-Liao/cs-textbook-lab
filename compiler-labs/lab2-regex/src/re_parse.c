/*
 * re_parse.c —— mini 正则表达式解析器实现
 *
 * 语法（EBNF，优先级从低到高——正是"递归下降"的经典分层，lab3 预习）：
 *   alt  → cat ('|' cat)*
 *   cat  → rep { rep }                 连接：至少一个（空正则另行处理）
 *   rep  → atom ('*' | '+' | '?')*
 *   atom → '(' alt ')' | '[' 类 ']' | '.' | '\' 转义 | 普通字符
 *
 * 有趣的对照：正则自己的文法就是用递归下降分析的（本文件），
 * 而正则描述的又是词法单元——"用编译器技术造编译器的工具"。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "re_parse.h"

/* ---------- 简单 arena：AST 节点只分配不释放（lab 生命期短） ---------- */
static Node *arena_alloc(void)
{
    static Node *pool = NULL;
    static int used = 0, cap = 0;
    if (used == cap) {
        cap = cap ? cap * 2 : 128;
        pool = malloc(sizeof(Node) * (size_t)cap); /* 简化：不回收旧池 */
        used = 0;
    }
    return &pool[used++];
}

static Node *new_node(NodeKind k)
{
    Node *n = arena_alloc();
    memset(n, 0, sizeof *n);
    n->kind = k;
    return n;
}

/* ---------- 解析器状态 ---------- */
static const char *re;   /* 正则字符串 */
static int         rp;   /* 当前下标 */
static int         re_err;

static void re_error(const char *msg)
{
    if (!re_err) { /* 只报第一个错误 */
        fprintf(stderr, "re 语法错误(位置 %d): %s\n", rp, msg);
        re_err = 1;
    }
}

static char cur(void)    { return re[rp]; }
static char peek2(void)  { return re[rp + 1]; }
static int  at_end(void) { return re[rp] == '\0'; }

/* ---------- atom ---------- */

/* 解析 [...] 字符类 → N_CLASS 节点 */
static Node *parse_class(void)
{
    Node *n = new_node(N_CLASS);
    rp++; /* 吃掉 '[' */
    if (cur() == '^') { n->neg = 1; rp++; }

    int first = 1;
    for (;;) {
        char c = cur();
        if (c == ']' && !first) { rp++; break; } /* ']' 结束 */
        if (c == '\0') { re_error("字符类缺少 ']'"); return NULL; }
        first = 0;

        /* 取一个类内字符（支持转义）。'\' 后必须还有字符——否则
         * cur() 读到 '\0' 后再 rp++，游标被推过串尾继续越界读。 */
        int lo;
        if (c == '\\') {
            rp++;
            if (cur() == '\0') { re_error("字符类以孤立的 '\\' 结尾"); return NULL; }
            switch (cur()) {
            case 'n': lo = '\n'; break;
            case 't': lo = '\t'; break;
            case 'r': lo = '\r'; break;
            default:  lo = cur(); break;
            }
            rp++;
        } else {
            lo = (unsigned char)c;
            rp++;
        }

        /* 范围 a-z（'-' 在最后/最前是普通字符） */
        if (cur() == '-' && peek2() != ']' && peek2() != '\0') {
            rp++; /* 吃掉 '-' */
            int hi;
            if (cur() == '\\') {
                rp++;
                if (cur() == '\0') { re_error("范围终点以孤立的 '\\' 结尾"); return NULL; }
                hi = (unsigned char)cur();
                rp++;
            }
            else { hi = (unsigned char)cur(); rp++; }
            if (hi < lo) { re_error("字符范围反了(如 z-a)"); return NULL; }
            for (int i = lo; i <= hi; i++)
                n->cls[i >> 3] |= (unsigned char)(1 << (i & 7));
        } else {
            n->cls[lo >> 3] |= (unsigned char)(1 << (lo & 7));
        }
    }
    return n;
}

static Node *parse_alt(void); /* 前置声明（互相递归） */

static Node *parse_atom(void)
{
    char c = cur();
    if (c == '(') {
        rp++;
        Node *inner = parse_alt();
        if (!inner) return NULL;
        if (cur() != ')') { re_error("缺少 ')'"); return NULL; }
        rp++;
        return inner;
    }
    if (c == '[') return parse_class();
    if (c == '.') { rp++; return new_node(N_ANY); }
    if (c == '\\') {
        rp++;
        if (at_end()) { re_error("以 '\\' 结尾"); return NULL; }
        int ch;
        switch (cur()) {
        case 'n': ch = '\n'; break;
        case 't': ch = '\t'; break;
        case 'r': ch = '\r'; break;
        default:  ch = (unsigned char)cur(); break;
        }
        rp++;
        Node *n = new_node(N_CHAR);
        n->ch = ch;
        return n;
    }
    if (c == '\0' || c == '|' || c == ')' || c == '*' || c == '+' || c == '?') {
        re_error("此处不应出现该字符（表达式为空或运算符缺少操作数？）");
        return NULL;
    }
    rp++;
    Node *n = new_node(N_CHAR);
    n->ch = (unsigned char)c;
    return n;
}

/* rep → atom ('*'|'+'|'?')*   后缀运算符可以叠：a** = (a*)* */
static Node *parse_rep(void)
{
    Node *n = parse_atom();
    if (!n) return NULL;
    while (cur() == '*' || cur() == '+' || cur() == '?') {
        Node *op = new_node(cur() == '*' ? N_STAR :
                            cur() == '+' ? N_PLUS : N_QUEST);
        op->l = n;
        n = op;
        rp++;
    }
    return n;
}

/* cat → rep { rep }：不断连接右边出现的项，直到碰上 '|' ')' 或结尾 */
static Node *parse_cat(void)
{
    Node *n = parse_rep();
    if (!n) return NULL;
    while (!at_end() && cur() != '|' && cur() != ')') {
        Node *r = parse_rep();
        if (!r) return NULL;
        Node *cat = new_node(N_CAT);
        cat->l = n;
        cat->r = r;
        n = cat;
    }
    return n;
}

/* alt → cat ('|' cat)* */
static Node *parse_alt(void)
{
    Node *n = parse_cat();
    if (!n) return NULL;
    while (cur() == '|') {
        rp++;
        Node *r = parse_cat();
        if (!r) return NULL;
        Node *alt = new_node(N_ALT);
        alt->l = n;
        alt->r = r;
        n = alt;
    }
    return n;
}

Node *re_parse(const char *pattern)
{
    re = pattern;
    rp = 0;
    re_err = 0;

    if (pattern[0] == '\0') return new_node(N_EMPTY); /* 空正则 = ε */

    Node *n = parse_alt();
    if (!n) return NULL;
    if (!at_end()) { /* 只可能是多余的 ')' */
        re_error("多余的 ')'");
        return NULL;
    }
    return n;
}
