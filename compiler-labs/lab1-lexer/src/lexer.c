/*
 * lexer.c —— 手写词法分析器实现（lab1，对照龙书第 3 章）
 *
 * 主函数 lexer_next() 的结构就是龙书 3.4 节"转移图"的文字化：
 * 每一类词法单元对应一张转移图，我们用 C 的 if/switch 手工实现
 * 这些转移（读一个字符 -> 决定去哪个状态 -> ... -> 到达接受状态）。
 *
 * 两条最重要的工程原则（龙书 3.4 节）：
 *   [1] 最长匹配(maximal munch)：能多吃一个字符构成合法记号就继续吃。
 *       所以 "==" 不会变成两个 "="，"123" 不会变成三个数字。
 *       实现：先按"宽"的类别（标识符/数字）扫描到底，再看回退。
 *   [2] 超前查看(lookahead)：判断 ">=" 这类双字符记号时，需要偷看
 *       下一个字符才能决定当前记号是什么；偷看了但不属于本记号时
 *       要"吐回去"（本实现用 pos 游标回退，等价于龙书说的回退）。
 *
 * 词法错误恢复（龙书 3.1 节）：词法级的恢复很朴素——报告错误、
 * 跳过出问题的字符、继续扫描，把所有错误一次性报完（而不是
 * 见到第一个错误就退出；对比 lab0 的"一错就停"）。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lexer.h"
#include "symtab.h"

static int had_error = 0;

/* ---------------- 游标原语：peek/advance ---------------- */

static char peek(Lexer *lx)   { return lx->src[lx->pos]; }
static char peek2(Lexer *lx)  { return lx->src[lx->pos + 1]; }

/* 前进一个字符并维护行列号。
 * 简化说明：列按"字节"计——UTF-8 中文一个字算 3 列；且把制表符
 * 也算 1 列。真实编译器会处理得更细（Clang 甚至能算出"显示列"）。 */
static char advance(Lexer *lx)
{
    char c = lx->src[lx->pos++];
    if (c == '\n') { lx->line++; lx->col = 1; }
    else           { lx->col++; }
    return c;
}

/* 显式坐标版：词法错误要在**消费字符之前**用记号起始位置报——
 * 先 advance 再报会让列号指到坏字符的下一格（off-by-one）。 */
static void lex_error_at(int line, int col, const char *msg)
{
    had_error = 1;
    fprintf(stderr, "line %d, col %d: error: %s\n", line, col, msg);
}

/* ---------------- 关键字表 ----------------
 * 龙书 3.4 节的经典问题："if" 满足标识符的正则，怎么区分关键字？
 * 答案（至今仍是标准做法）：先按标识符扫描，然后查保留字表。
 * 10 个关键字线性查找足够；工业界用 trie/完美哈希(如 gperf)。
 */
static const struct { const char *kw; TokenType type; } KEYWORDS[] = {
    {"int", T_KW_INT}, {"float", T_KW_FLOAT}, {"void", T_KW_VOID},
    {"if", T_KW_IF}, {"else", T_KW_ELSE}, {"while", T_KW_WHILE},
    {"return", T_KW_RETURN}, {"break", T_KW_BREAK},
    {"continue", T_KW_CONTINUE}, {"print", T_KW_PRINT},
};
/* keyword_lookup 用 t.type - T_KW_INT 当数组下标，依赖枚举连续且与
 * 本表顺序严格一致——编译期钉死，谁调乱枚举立刻炸在构建期。 */
_Static_assert(sizeof KEYWORDS / sizeof KEYWORDS[0]
                   == T_KW_PRINT - T_KW_INT + 1,
               "KEYWORDS 表必须覆盖 T_KW_INT..T_KW_PRINT 且顺序一致");

static TokenType keyword_lookup(const char *s, int len)
{
    for (unsigned i = 0; i < sizeof KEYWORDS / sizeof KEYWORDS[0]; i++)
        if ((int)strlen(KEYWORDS[i].kw) == len &&
            strncmp(KEYWORDS[i].kw, s, len) == 0)
            return KEYWORDS[i].type;
    return T_IDENT; /* 不是关键字就是普通标识符 */
}

void lexer_init(Lexer *lx, const char *src)
{
    lx->src = src;
    lx->pos = 0;
    lx->line = 1;
    lx->col = 1;
}

int lexer_had_error(void) { return had_error; }

/* ---------------- 扫描一个记号 ----------------
 * 返回 T_ERROR 表示"这个位置有词法错误，已恢复（跳过坏字符），
 * 请继续调用"。到达文件结尾后永远返回 T_EOF。 */
Token lexer_next(Lexer *lx)
{
    Token t = {0};
    t.line = lx->line;
    t.col = lx->col;

    /* ===== 步骤 0：跳过空白与注释 ===== */
    for (;;) {
        char c = peek(lx);
        if (c == '\0') { t.type = T_EOF; t.lexeme = "<EOF>"; return t; }

        if (isspace((unsigned char)c)) { advance(lx); continue; }

        /* 行注释 //...：跳到行尾 */
        if (c == '/' && peek2(lx) == '/') {
            while (peek(lx) != '\0' && peek(lx) != '\n') advance(lx);
            continue;
        }
        /* 块注释 (* ... *)：注意块注释可以换行、可以包含 '*' */
        if (c == '/' && peek2(lx) == '*') {
            int start_line = lx->line, start_col = lx->col;
            advance(lx); advance(lx); /* 吃掉 "/ *" */
            for (;;) {
                if (peek(lx) == '\0') {
                    had_error = 1;
                    /* 报错位置用注释开头而不是文件结尾，更方便定位 */
                    fprintf(stderr,
                        "line %d, col %d: error: 块注释未闭合(从注释开头报错)\n",
                        start_line, start_col);
                    t.type = T_ERROR; t.lexeme = "<bad comment>";
                    return t;
                }
                if (peek(lx) == '*' && peek2(lx) == '/') {
                    advance(lx); advance(lx);
                    break; /* 注释正常结束，回到外层循环继续跳空白 */
                }
                advance(lx);
            }
            continue;
        }
        break; /* 既不是空白也不是注释：从这里开始真正的扫描 */
    }

    /* 记录记号真正开始的位置（跳完空白后） */
    t.line = lx->line;
    t.col = lx->col;
    int start = lx->pos;
    char c = peek(lx);

    /* ===== 标识符 / 关键字: [A-Za-z_][A-Za-z0-9_]* =====
     * 转移图：状态0(字母/_)->状态1，状态1 上吃 [A-Za-z0-9_] 自环，
     * 其他字符出->接受。对照龙书图 3.x"标识符的转移图"。 */
    if (isalpha((unsigned char)c) || c == '_') {
        while (isalnum((unsigned char)peek(lx)) || peek(lx) == '_')
            advance(lx);
        int len = lx->pos - start;
        t.type = keyword_lookup(lx->src + start, len);
        /* 驻留进符号表（去重；关键字不走驻留，它们只有 10 个定值） */
        if (t.type == T_IDENT)
            t.lexeme = symtab_intern(lx->src + start, len, t.line);
        else
            t.lexeme = KEYWORDS[t.type - T_KW_INT].kw;
        return t;
    }

    /* ===== 数字: [0-9]+(\.[0-9]*)?  或  .[0-9]+ =====
     * 整数走整型，见到 '.' 变浮点。最麻烦的是"病态输入"：
     * 1.2.3 / 123abc —— 龙书叫"词法错误"，我们在原地报错并跳过。 */
    if (isdigit((unsigned char)c) ||
        (c == '.' && isdigit((unsigned char)peek2(lx)))) {
        int is_float = 0;
        while (isdigit((unsigned char)peek(lx))) advance(lx);
        if (peek(lx) == '.') {
            is_float = 1;
            advance(lx);
            while (isdigit((unsigned char)peek(lx))) advance(lx);
        }
        /* 病态检查 1：后面还有 '.'（如 1.2.3） */
        if (peek(lx) == '.') {
            while (isdigit((unsigned char)peek(lx)) || peek(lx) == '.')
                advance(lx); /* 把整串烂数字吃干净，避免连锁报错 */
            lex_error_at(t.line, t.col, "非法数字字面量（多个小数点？）");
            t.type = T_ERROR; t.lexeme = "<bad number>";
            return t;
        }
        /* 病态检查 2：**整数**后直接跟字母（如 123abc）。带点的浮点
         * 不在此列——按规约 [0-9]+\.[0-9]* 与最长匹配，"3.f" 就是
         * FLOAT("3.") 后跟标识符 f（与 lab2 的 mini-flex 一致）。 */
        if (!is_float &&
            (isalpha((unsigned char)peek(lx)) || peek(lx) == '_')) {
            while (isalnum((unsigned char)peek(lx)) || peek(lx) == '_')
                advance(lx);
            lex_error_at(t.line, t.col,
                         "数字字面量后不能直接跟字母（漏了空格？）");
            t.type = T_ERROR; t.lexeme = "<bad number>";
            return t;
        }

        int len = lx->pos - start;
        char *buf = malloc((size_t)len + 1);
        memcpy(buf, lx->src + start, len);
        buf[len] = '\0';
        t.lexeme = buf;
        if (is_float) { t.type = T_FLOAT_LIT; t.dval = strtod(buf, NULL); }
        else          { t.type = T_INT_LIT;   t.ival = strtol(buf, NULL, 10); }
        return t;
    }

    /* ===== 运算符与标点（最长匹配 + 超前查看） ===== */
    switch (c) {
    case '+': advance(lx); t.type = T_PLUS;    t.lexeme = "+";  return t;
    case '-': advance(lx); t.type = T_MINUS;   t.lexeme = "-";  return t;
    case '*': advance(lx); t.type = T_STAR;    t.lexeme = "*";  return t;
    case '%': advance(lx); t.type = T_PERCENT; t.lexeme = "%";  return t;

    case '/': advance(lx); t.type = T_SLASH;   t.lexeme = "/";  return t;

    case '=':
        advance(lx);
        if (peek(lx) == '=') { advance(lx); t.type = T_EQ; t.lexeme = "=="; }
        else                 { t.type = T_ASSIGN; t.lexeme = "="; }
        return t;                     /* <-- 超前查看一个字符决定单双 */

    case '!':
        advance(lx);
        if (peek(lx) == '=') { advance(lx); t.type = T_NEQ; t.lexeme = "!="; }
        else                 { t.type = T_NOT; t.lexeme = "!"; }
        return t;

    case '<':
        advance(lx);
        if (peek(lx) == '=') { advance(lx); t.type = T_LE; t.lexeme = "<="; }
        else                 { t.type = T_LT; t.lexeme = "<"; }
        return t;

    case '>':
        advance(lx);
        if (peek(lx) == '=') { advance(lx); t.type = T_GE; t.lexeme = ">="; }
        else                 { t.type = T_GT; t.lexeme = ">"; }
        return t;

    case '&':
        advance(lx);
        if (peek(lx) == '&') { advance(lx); t.type = T_ANDAND; t.lexeme = "&&"; return t; }
        lex_error_at(t.line, t.col,
                     "MiniC 不支持单字符 '&'（只支持逻辑与 '&&'）");
        t.type = T_ERROR; t.lexeme = "<bad op>"; return t;

    case '|':
        advance(lx);
        if (peek(lx) == '|') { advance(lx); t.type = T_OROR; t.lexeme = "||"; return t; }
        lex_error_at(t.line, t.col,
                     "MiniC 不支持单字符 '|'（只支持逻辑或 '||'）");
        t.type = T_ERROR; t.lexeme = "<bad op>"; return t;

    case '(': advance(lx); t.type = T_LPAREN;   t.lexeme = "("; return t;
    case ')': advance(lx); t.type = T_RPAREN;   t.lexeme = ")"; return t;
    case '{': advance(lx); t.type = T_LBRACE;   t.lexeme = "{"; return t;
    case '}': advance(lx); t.type = T_RBRACE;   t.lexeme = "}"; return t;
    case '[': advance(lx); t.type = T_LBRACKET; t.lexeme = "["; return t;
    case ']': advance(lx); t.type = T_RBRACKET; t.lexeme = "]"; return t;
    case ';': advance(lx); t.type = T_SEMI;     t.lexeme = ";"; return t;
    case ',': advance(lx); t.type = T_COMMA;    t.lexeme = ","; return t;

    default:
        /* ===== 非法字符：典型的词法错误恢复——先按记号起始列报错，
         * 再消费跳过 ===== */
        {
            char msg[64];
            snprintf(msg, sizeof msg,
                     "非法字符 '%c'（中文全角标点是常见误因）", c);
            lex_error_at(t.line, t.col, msg);
        }
        advance(lx);
        t.type = T_ERROR; t.lexeme = "<bad char>";
        return t;
    }
}

/* 类别 -> 名字表。用宏表驱动减少重复（顺序必须和 token.h 的枚举一致） */
const char *token_name(TokenType t)
{
    static const char *NAMES[] = {
        "T_EOF",
        "T_KW_INT", "T_KW_FLOAT", "T_KW_VOID",
        "T_KW_IF", "T_KW_ELSE", "T_KW_WHILE", "T_KW_RETURN",
        "T_KW_BREAK", "T_KW_CONTINUE", "T_KW_PRINT",
        "T_INT_LIT", "T_FLOAT_LIT", "T_IDENT",
        "T_PLUS", "T_MINUS", "T_STAR", "T_SLASH", "T_PERCENT",
        "T_ASSIGN", "T_EQ", "T_NEQ", "T_LT", "T_LE", "T_GT", "T_GE",
        "T_NOT", "T_ANDAND", "T_OROR",
        "T_LPAREN", "T_RPAREN", "T_LBRACE", "T_RBRACE",
        "T_LBRACKET", "T_RBRACKET", "T_SEMI", "T_COMMA",
        "T_ERROR",
    };
    return NAMES[t];
}
