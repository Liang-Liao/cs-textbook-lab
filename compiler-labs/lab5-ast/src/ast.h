/*
 * ast.h —— 抽象语法树（AST）的节点定义与操作（对照龙书 5.1/5.3 节）
 *
 * lab3 的递归下降在语法分析的同时**直接求值**——值沿着函数返回值
 * 一路上抛，语句结束就没了。要让"分析结果"留下来供后续阶段使用
 * （lab6 生成三地址码、lab9 优化），就要把每个产生式的翻译结果
 * 物化成一棵树：这就是 AST。
 *
 * 语法制导翻译的标准形状（龙书 5.1，本 lab 的核心思想）：
 *   - **综合属性**（自下而上）：孩子的信息汇到父节点 —— 对应每个
 *     parse 函数的**返回值**（一个 AstNode*，其中 type 字段就是
 *     表达式的类型这个综合属性）；
 *   - **继承属性**（自上而下）：父节点传给孩子的上下文 —— 对应
 *     parse 函数的**参数**（如 parse_stmt(loop_depth) 告诉子语句
 *     "你在不在循环里"，break/continue 检查靠它）。
 *
 * 节点用一个结构体 + kind 判别（而非每 kind 一个 struct 的 union）：
 * 教学代码优先"一个 struct 读到底"，字段不用的就空着。
 */
#ifndef AST_H
#define AST_H

#include "symtab.h"   /* Type 与 Sym 都定义在那里（symtab 是 ast 的下层） */
#include "token.h"    /* TokenType：A_BINOP 记录自己是哪个运算符 */

typedef enum {
    /* ---- 表达式节点（type 字段有效 = 综合属性） ---- */
    A_INT_LIT,    /* (int 3)          —— ival */
    A_FLOAT_LIT,  /* (float 2.5)      —— dval */
    A_VAR,        /* (var x)          —— sym 指向符号表条目（名字解析结果） */
    A_NEG,        /* (neg e)          —— 一元负号，a */
    A_BINOP,      /* (binop + l r)    —— op/a/b；算术与比较共用 */
    A_I2F,        /* (i2f e)          —— 编译器插入的 int→float 隐式提升 */
    A_ERR,        /* (error)          —— 错误恢复占位（type=TY_ERR，不会被执行） */

    /* ---- 语句节点（type 字段无意义） ---- */
    A_PROG,       /* (program ...)    —— 根：语句链 next */
    A_DECL,       /* (decl int x e)   —— 声明，sym + 可选初始化式 a */
    A_ASSIGN,     /* (assign x e)     —— 赋值，sym（左值）+ a（右值） */
    A_PRINT,      /* (print e)        —— a */
    A_EXPRSTMT,   /* (exprst e)       —— 裸表达式语句 `1+1;`，a（值被丢弃） */
    A_IF,         /* (if c t e)       —— a=条件 b=then c=else（可 NULL） */
    A_WHILE,      /* (while c b)      —— a=条件 b=循环体 */
    A_BREAK,      /* (break) */
    A_CONTINUE,   /* (continue) */
    A_BLOCK,      /* (block ...)      —— { }：a 指向语句链头（对应一个作用域） */
    A_EMPTY       /* (empty)          —— 空语句 `;` */
} AstKind;

typedef struct AstNode {
    AstKind     kind;
    Type        type;   /* 表达式类型：TY_INT/TY_FLOAT/TY_ERR（综合属性） */
    int         line;   /* 节点首记号的行号（运行时报错定位） */
    const char *name;   /* A_DECL/A_ASSIGN/A_VAR：变量名（指向词法器的词素） */
    Sym        *sym;    /* 同上三个 kind：名字解析出的符号表条目（A_VAR 专属；
                           声明重复时报错后 sym 可能为 NULL，此时只留 name） */
    long        ival;   /* A_INT_LIT */
    double      dval;   /* A_FLOAT_LIT */
    TokenType   op;     /* A_BINOP：+ - * / % < <= > >= == != */
    struct AstNode *a, *b, *c;  /* 孩子（含义见各 kind 注释） */
    struct AstNode *next;       /* 语句链（A_PROG/A_BLOCK 串兄弟语句） */
} AstNode;

/* ---- 构造（全部 malloc 一个节点并填字段；调用方负责接线） ---- */
AstNode *ast_int(long v, int line);
AstNode *ast_float(double v, int line);
AstNode *ast_var(Sym *sym, int line);
AstNode *ast_neg(AstNode *a, int line);                 /* type 取自 a */
AstNode *ast_binop(TokenType op, AstNode *a, AstNode *b,
                   Type t, int line);
AstNode *ast_i2f(AstNode *a, int line);                 /* type=TY_FLOAT */
AstNode *ast_err(int line);                             /* 错误占位 */

AstNode *ast_prog(AstNode *first_stmt);
AstNode *ast_decl(Sym *sym, AstNode *init, int line);
AstNode *ast_assign(Sym *sym, AstNode *rhs, int line);
AstNode *ast_print(AstNode *e, int line);
AstNode *ast_exprstmt(AstNode *e, int line);
AstNode *ast_if(AstNode *cond, AstNode *th, AstNode *el, int line);
AstNode *ast_while(AstNode *cond, AstNode *body, int line);
AstNode *ast_break(int line);
AstNode *ast_continue(int line);
AstNode *ast_block(AstNode *first_stmt, int line);
AstNode *ast_empty(int line);

/* ---- dump：S-表达式，缩进多行，纯 ASCII（机器可对拍） ---- */
void ast_dump(const AstNode *prog);

/* ---- 释放：递归 free 节点本身。
 * 注意 name/sym 指向的字符串与 Sym 条目**不**在这里释放——它们的
 * 生命周期由词法器/符号表管理（一直活到进程结束，见 symtab.h）。 */
void ast_free(AstNode *prog);

#endif /* AST_H */
