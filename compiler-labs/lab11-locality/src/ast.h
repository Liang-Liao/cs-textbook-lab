/*
 * ast.h —— 抽象语法树（AST）的节点定义与操作（对照龙书 5.1/5.3 节）
 *
 * lab5 建立了"语法制导翻译的产物 = 一棵树"；lab6 在树上加了短路布尔
 * （A_AND/A_OR/A_NOT）、过程调用（A_CALL/A_RETURN/A_FUNCDEF）与数组
 * 两种节点（A_INDEX 读 / A_STORE 写，6.4 节数组翻译的 AST 形态）。
 *
 * 沿用 lab5 的全部约定：综合属性=parse 函数返回值（type 字段）、
 * 继承属性=parse 函数参数、A_VAR 直接挂 Sym*（名字解析编译期完成）。
 */
#ifndef AST_H
#define AST_H

#include "symtab.h"
#include "token.h"

typedef enum {
    /* ---- 表达式节点（type 字段有效 = 综合属性） ---- */
    A_INT_LIT,    /* (int 3)          —— ival */
    A_FLOAT_LIT,  /* (float 2.5)      —— dval */
    A_VAR,        /* (var x)          —— sym 指向符号表条目 */
    A_INDEX,      /* (index a[i])     —— 数组读：sym=数组符号，a=下标；
                                  *   lab11 二维：c=列下标；type = 元素类型 */
    A_NEG,        /* (neg e)          —— 一元负号，a */
    A_NOT,        /* (not e)          —— 逻辑非（短路无关，仅值翻转），a */
    A_BINOP,      /* (binop + l r)    —— op/a/b；算术与比较共用 */
    A_AND,        /* (and l r)        —— 短路与；求值顺序：左可能短路右 */
    A_OR,         /* (or l r)         —— 短路或 */
    A_I2F,        /* (i2f e)          —— 编译器插入的 int→float 隐式提升 */
    A_CALL,       /* (call f e1 ...)  —— sym=函数符号；a 起用 next 串实参 */
    A_ERR,        /* (error)          —— 错误恢复占位 */

    /* ---- 语句节点（type 字段无意义） ---- */
    A_PROG,       /* (program ...)    —— 根：语句链 next（含 A_FUNCDEF） */
    A_DECL,       /* (decl int x e)   —— sym + 可选初始化式 a
                                  *   （sym->arr_len>0 = 数组声明，无 init） */
    A_ASSIGN,     /* (assign x e)     —— sym（左值）+ a（右值） */
    A_STORE,      /* (store a[i] e)   —— 数组元素赋值：sym=数组符号，
                                  *   a=下标，b=右值；lab11 二维：c=列下标 */
    A_PRINT,      /* (print e)        —— a */
    A_EXPRSTMT,   /* (exprst e)       —— 裸表达式语句（含 `f();`），a */
    A_IF,         /* (if c t e)       —— a=条件 b=then c=else（可 NULL） */
    A_WHILE,      /* (while c b)      —— a=条件 b=循环体 */
    A_BREAK,      /* (break) */
    A_CONTINUE,   /* (continue) */
    A_BLOCK,      /* (block ...)      —— { }：a 指向语句链头（一个作用域） */
    A_EMPTY,      /* (empty) */
    A_RETURN,     /* (return [e])     —— a 可 NULL（void 函数的裸 return） */
    A_FUNCDEF     /* (func int f(int a, ...) (block ...)) —— sym + a=函数体 */
} AstKind;

typedef struct AstNode {
    AstKind     kind;
    Type        type;
    int         line;
    const char *name;
    Sym        *sym;
    long        ival;
    double      dval;
    TokenType   op;
    struct AstNode *a, *b, *c;
    struct AstNode *next;
    Sym        *psym[MAX_PARAMS];   /* A_FUNCDEF：形参符号（按序，
                                     * eval 绑定实参 / gen 分配 IR 名用） */
} AstNode;

/* ---- 构造 ---- */
AstNode *ast_int(long v, int line);
AstNode *ast_float(double v, int line);
AstNode *ast_var(Sym *sym, int line);
AstNode *ast_index(Sym *arr, AstNode *idx, int line);   /* a[i]：type=元素类型 */
/* lab11：二维数组读 m[i][j]。槽位约定：**c 槽固定是列下标**（A_INDEX
 * 与 A_STORE 一致——b 槽在 A_STORE 已被右值占用，统一挪到 c 免混淆） */
AstNode *ast_index2(Sym *arr, AstNode *row, AstNode *col, int line);
AstNode *ast_neg(AstNode *a, int line);
AstNode *ast_not(AstNode *a, int line);                 /* 结果类型 int 0/1 */
AstNode *ast_binop(TokenType op, AstNode *a, AstNode *b,
                   Type t, int line);
AstNode *ast_logic(AstKind and_or, AstNode *a, AstNode *b, int line);
AstNode *ast_i2f(AstNode *a, int line);
AstNode *ast_call(Sym *sym, AstNode *first_arg, int line);
AstNode *ast_err(int line);

AstNode *ast_prog(AstNode *first_stmt);
AstNode *ast_decl(Sym *sym, AstNode *init, int line);
AstNode *ast_assign(Sym *sym, AstNode *rhs, int line);
AstNode *ast_store(Sym *arr, AstNode *idx, AstNode *rhs, int line);
/* lab11：二维数组写 m[i][j] = e（a=行下标，c=列下标，b=右值） */
AstNode *ast_store2(Sym *arr, AstNode *row, AstNode *col, AstNode *rhs,
                    int line);
AstNode *ast_print(AstNode *e, int line);
AstNode *ast_exprstmt(AstNode *e, int line);
AstNode *ast_if(AstNode *cond, AstNode *th, AstNode *el, int line);
AstNode *ast_while(AstNode *cond, AstNode *body, int line);
AstNode *ast_break(int line);
AstNode *ast_continue(int line);
AstNode *ast_block(AstNode *first_stmt, int line);
AstNode *ast_empty(int line);
AstNode *ast_return(AstNode *e, int line);
AstNode *ast_funcdef(Sym *sym, AstNode *body,
                     Sym **params, int nparams, int line);

/* ---- dump：S-表达式，缩进多行，纯 ASCII ---- */
void ast_dump(const AstNode *prog);

/* ---- 释放：递归 free 节点（name/Sym 生命周期更长，见 lab5） ---- */
void ast_free(AstNode *prog);

#endif /* AST_H */
