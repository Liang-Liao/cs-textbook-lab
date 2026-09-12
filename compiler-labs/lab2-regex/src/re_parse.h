/*
 * re_parse.h —— mini 正则表达式解析器：把正则字符串解析成 AST
 * （lab2，对照龙书 3.3 节"词法单元的规约"）
 *
 * 支持的语法（足够描述 MiniC 的全部记号）：
 *   x|y     选择        x*y     闭包(0 次或多次)
 *   xy      连接        x+y     正闭包(1 次或多次)
 *   (x)     分组        x?      可选(0 次或 1 次)
 *   [abc] [a-z] [^ab]    字符类（支持范围和取反）
 *   .       除换行外任意字符
 *   \*      反斜杠转义任意特殊字符（\n \t \r 也认）
 *   （没有的常用特性：{m,n} 计数、反向引用、前瞻——见 README 练习）
 */
#ifndef RE_PARSE_H
#define RE_PARSE_H

/* AST 节点种类 */
typedef enum {
    N_CHAR,   /* 单个字符（ch） */
    N_CLASS,  /* 字符类（256 位位图 cls + 取反标志 neg） */
    N_ANY,    /* '.' 任意字符（除 '\n'） */
    N_ALT,    /* l | r */
    N_CAT,    /* l r  （连接） */
    N_STAR,   /* l* */
    N_PLUS,   /* l+ */
    N_QUEST,  /* l? */
    N_EMPTY   /* ε 空串（空正则用） */
} NodeKind;

typedef struct Node {
    NodeKind kind;
    int ch;               /* N_CHAR */
    unsigned char cls[32];/* N_CLASS: 256 位位图，cls[c>>3] & (1<<(c&7)) */
    int neg;              /* N_CLASS: 1 表示 [^...] 取反 */
    struct Node *l, *r;   /* 一元节点只用 l；ALT/CAT 用 l、r */
} Node;

/* 解析正则字符串。成功返回 AST 根（用内部 arena 分配，不释放），
 * 失败返回 NULL 并在 stderr 报告位置与原因。 */
Node *re_parse(const char *pattern);

#endif /* RE_PARSE_H */
