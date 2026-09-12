/*
 * lexer.c —— lab5 用的词法分析器（lab4 的原样复制；lab4 又是 lab1 的
 * 精简复刻：去掉符号表驻留，词素直接 strdup，其余行为与 lab1 完全一致，
 * 详细注释见 lab1）。放在这里是为了让 lab5 完全自包含、可独立阅读。
 *
 * lab5 升级的是语法分析**之后**的阶段（AST/符号表/类型检查），词法层
 * 与 lab3/lab4 逐字节一致——这正是编译器分阶段设计的意义：前端的前段
 * 稳定后，后段可以独立演进。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lexer.h"

static int had_error = 0;

static char peek(Lexer *lx)  { return lx->src[lx->pos]; }
static char peek2(Lexer *lx) { return lx->src[lx->pos + 1]; }

static char advance(Lexer *lx)
{
    char c = lx->src[lx->pos++];
    if (c == '\n') { lx->line++; lx->col = 1; }
    else           { lx->col++; }
    return c;
}

static void lex_error(Lexer *lx, const char *msg)
{
    had_error = 1;
    fprintf(stderr, "line %d, col %d: error: %s\n",
            lx->line, lx->col, msg);
}

static const struct { const char *kw; TokenType type; } KEYWORDS[] = {
    {"int", T_KW_INT}, {"float", T_KW_FLOAT}, {"void", T_KW_VOID},
    {"if", T_KW_IF}, {"else", T_KW_ELSE}, {"while", T_KW_WHILE},
    {"return", T_KW_RETURN}, {"break", T_KW_BREAK},
    {"continue", T_KW_CONTINUE}, {"print", T_KW_PRINT},
};

static TokenType keyword_lookup(const char *s, int len)
{
    for (unsigned i = 0; i < sizeof KEYWORDS / sizeof KEYWORDS[0]; i++)
        if ((int)strlen(KEYWORDS[i].kw) == len &&
            strncmp(KEYWORDS[i].kw, s, len) == 0)
            return KEYWORDS[i].type;
    return T_IDENT;
}

void lexer_init(Lexer *lx, const char *src)
{
    lx->src = src;
    lx->pos = 0;
    lx->line = 1;
    lx->col = 1;
}

int lexer_had_error(void) { return had_error; }

/* 取记号时顺便过滤 T_ERROR（词法错误已由词法器报告并跳过；
 * 语法分析器只关心"干净"的记号流——这是前后端衔接的常见做法） */
Token lexer_next_clean(Lexer *lx)
{
    for (;;) {
        Token t = lexer_next(lx);
        if (t.type != T_ERROR) return t;
    }
}

Token lexer_next(Lexer *lx)
{
    Token t = {0};
    t.line = lx->line;
    t.col = lx->col;

    for (;;) {
        char c = peek(lx);
        if (c == '\0') { t.type = T_EOF; t.lexeme = "<EOF>"; return t; }
        if (isspace((unsigned char)c)) { advance(lx); continue; }
        if (c == '/' && peek2(lx) == '/') {
            while (peek(lx) != '\0' && peek(lx) != '\n') advance(lx);
            continue;
        }
        if (c == '/' && peek2(lx) == '*') {
            int sl = lx->line, sc = lx->col;
            advance(lx); advance(lx);
            for (;;) {
                if (peek(lx) == '\0') {
                    had_error = 1;
                    fprintf(stderr,
                        "line %d, col %d: error: 块注释未闭合\n", sl, sc);
                    t.type = T_ERROR; t.lexeme = "<bad>";
                    return t;
                }
                if (peek(lx) == '*' && peek2(lx) == '/') {
                    advance(lx); advance(lx);
                    break;
                }
                advance(lx);
            }
            continue;
        }
        break;
    }

    t.line = lx->line;
    t.col = lx->col;
    int start = lx->pos;
    char c = peek(lx);

    if (isalpha((unsigned char)c) || c == '_') {
        while (isalnum((unsigned char)peek(lx)) || peek(lx) == '_')
            advance(lx);
        int len = lx->pos - start;
        t.type = keyword_lookup(lx->src + start, len);
        char *s = malloc((size_t)len + 1);
        memcpy(s, lx->src + start, len);
        s[len] = '\0';
        t.lexeme = s;
        return t;
    }

    if (isdigit((unsigned char)c) ||
        (c == '.' && isdigit((unsigned char)peek2(lx)))) {
        int is_float = 0;
        while (isdigit((unsigned char)peek(lx))) advance(lx);
        if (peek(lx) == '.') { is_float = 1; advance(lx);
            while (isdigit((unsigned char)peek(lx))) advance(lx); }
        if (peek(lx) == '.') {
            while (isdigit((unsigned char)peek(lx)) || peek(lx) == '.')
                advance(lx);
            lex_error(lx, "非法数字字面量（多个小数点？）");
            t.type = T_ERROR; t.lexeme = "<bad>";
            return t;
        }
        if (isalpha((unsigned char)peek(lx)) || peek(lx) == '_') {
            while (isalnum((unsigned char)peek(lx)) || peek(lx) == '_')
                advance(lx);
            lex_error(lx, "数字字面量后不能直接跟字母");
            t.type = T_ERROR; t.lexeme = "<bad>";
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
        return t;
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
        lex_error(lx, "不支持单字符 '&'");
        t.type = T_ERROR; t.lexeme = "<bad>"; return t;
    case '|':
        advance(lx);
        if (peek(lx) == '|') { advance(lx); t.type = T_OROR; t.lexeme = "||"; return t; }
        lex_error(lx, "不支持单字符 '|'");
        t.type = T_ERROR; t.lexeme = "<bad>"; return t;
    case '(': advance(lx); t.type = T_LPAREN;   t.lexeme = "("; return t;
    case ')': advance(lx); t.type = T_RPAREN;   t.lexeme = ")"; return t;
    case '{': advance(lx); t.type = T_LBRACE;   t.lexeme = "{"; return t;
    case '}': advance(lx); t.type = T_RBRACE;   t.lexeme = "}"; return t;
    case '[': advance(lx); t.type = T_LBRACKET; t.lexeme = "["; return t;
    case ']': advance(lx); t.type = T_RBRACKET; t.lexeme = "]"; return t;
    case ';': advance(lx); t.type = T_SEMI;     t.lexeme = ";"; return t;
    case ',': advance(lx); t.type = T_COMMA;    t.lexeme = ","; return t;
    default:
        advance(lx);
        {
            char msg[64];
            snprintf(msg, sizeof msg, "非法字符 '%c'", c);
            lex_error(lx, msg);
        }
        t.type = T_ERROR; t.lexeme = "<bad>";
        return t;
    }
}

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
