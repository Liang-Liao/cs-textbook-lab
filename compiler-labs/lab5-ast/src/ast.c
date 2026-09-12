/*
 * ast.c —— AST 节点的构造、S-表达式 dump、释放（对照龙书 5.1/5.3 节）
 *
 * dump 输出是 Lisp 风格的 S-表达式（前缀式），例如：
 *   (program
 *     (decl int x (int 3))
 *     (assign x (binop + (var x) (int 1)))
 *     (while (binop < (var x) (int 5))
 *       (assign x (binop + (var x) (int 1))))
 *   )
 * 好处：无歧义（括号显式给出树形）、纯 ASCII 可对拍、和 Clang 的
 * `clang -Xclang -ast-dump` 输出（也是 S-表达式风格）神似——真实
 * 编译器调试 AST 用的就是这种东西。
 */
#include <stdio.h>
#include <stdlib.h>
#include "ast.h"

/* 通用分配：calloc 保证未用字段一律零值，杜绝"忘了初始化"一类 bug */
static AstNode *ast_new(AstKind k, int line)
{
    AstNode *n = calloc(1, sizeof *n);
    n->kind = k;
    n->line = line;
    return n;
}

/* ---------------- 表达式节点 ---------------- */

AstNode *ast_int(long v, int line)
{
    AstNode *n = ast_new(A_INT_LIT, line);
    n->type = TY_INT;      /* 字面量的类型：词法就知道，最底层的综合属性 */
    n->ival = v;
    return n;
}

AstNode *ast_float(double v, int line)
{
    AstNode *n = ast_new(A_FLOAT_LIT, line);
    n->type = TY_FLOAT;
    n->dval = v;
    return n;
}

AstNode *ast_var(Sym *sym, int line)
{
    AstNode *n = ast_new(A_VAR, line);
    n->type = sym->type;   /* 变量的类型 = 声明时写进符号表的类型 */
    n->name = sym->name;
    n->sym  = sym;         /* 名字解析在此完成：之后所有阶段只认 Sym* */
    return n;
}

AstNode *ast_neg(AstNode *a, int line)
{
    AstNode *n = ast_new(A_NEG, line);
    n->type = a->type;     /* 一元负号不改变类型 */
    n->a = a;
    return n;
}

AstNode *ast_binop(TokenType op, AstNode *a, AstNode *b, Type t, int line)
{
    AstNode *n = ast_new(A_BINOP, line);
    n->type = t;           /* 类型由调用方（parser 的类型检查）算好传入 */
    n->op = op;
    n->a = a;
    n->b = b;
    return n;
}

AstNode *ast_i2f(AstNode *a, int line)
{
    AstNode *n = ast_new(A_I2F, line);
    n->type = TY_FLOAT;
    n->a = a;
    return n;
}

AstNode *ast_err(int line)
{
    AstNode *n = ast_new(A_ERR, line);
    n->type = TY_ERR;      /* 错误占位：不再引发级联报错，也永远不会被执行 */
    return n;
}

/* ---------------- 语句节点 ---------------- */

AstNode *ast_prog(AstNode *first_stmt)
{
    AstNode *n = ast_new(A_PROG, 1);
    n->a = first_stmt;
    return n;
}

AstNode *ast_decl(Sym *sym, AstNode *init, int line)
{
    AstNode *n = ast_new(A_DECL, line);
    if (sym) n->name = sym->name;   /* 重复声明报错后 sym 可能为 NULL */
    n->sym  = sym;
    n->a    = init;                  /* 可为 NULL（无初始化式） */
    return n;
}

AstNode *ast_assign(Sym *sym, AstNode *rhs, int line)
{
    AstNode *n = ast_new(A_ASSIGN, line);
    if (sym) n->name = sym->name;
    n->sym = sym;
    n->a   = rhs;
    return n;
}

AstNode *ast_print(AstNode *e, int line)
{
    AstNode *n = ast_new(A_PRINT, line);
    n->a = e;
    return n;
}

AstNode *ast_exprstmt(AstNode *e, int line)
{
    AstNode *n = ast_new(A_EXPRSTMT, line);
    n->a = e;
    return n;
}

AstNode *ast_if(AstNode *cond, AstNode *th, AstNode *el, int line)
{
    AstNode *n = ast_new(A_IF, line);
    n->a = cond;
    n->b = th;
    n->c = el;             /* else 分支可省，此时为 NULL */
    return n;
}

AstNode *ast_while(AstNode *cond, AstNode *body, int line)
{
    AstNode *n = ast_new(A_WHILE, line);
    n->a = cond;
    n->b = body;
    return n;
}

AstNode *ast_break(int line)    { return ast_new(A_BREAK, line); }
AstNode *ast_continue(int line) { return ast_new(A_CONTINUE, line); }

AstNode *ast_block(AstNode *first_stmt, int line)
{
    AstNode *n = ast_new(A_BLOCK, line);
    n->a = first_stmt;
    return n;
}

AstNode *ast_empty(int line) { return ast_new(A_EMPTY, line); }

/* ================= dump：S-表达式 ================= */

/* 运算符的可打印形式（机器可对拍，保持 ASCII 符号本身） */
static const char *op_str(TokenType op)
{
    switch (op) {
    case T_PLUS:    return "+";
    case T_MINUS:   return "-";
    case T_STAR:    return "*";
    case T_SLASH:   return "/";
    case T_PERCENT: return "%";
    case T_LT:      return "<";
    case T_LE:      return "<=";
    case T_GT:      return ">";
    case T_GE:      return ">=";
    case T_EQ:      return "==";
    case T_NEQ:     return "!=";
    default:        return "?";
    }
}

/* 表达式打印成**单行**前缀式（语句才换行缩进，表达式跟着语句走） */
static void dump_expr(const AstNode *e)
{
    switch (e->kind) {
    case A_INT_LIT:   printf("(int %ld)", e->ival); break;
    case A_FLOAT_LIT: printf("(float %g)", e->dval); break;
    case A_VAR:       printf("(var %s)", e->name); break;
    case A_NEG:       printf("(neg "); dump_expr(e->a); printf(")"); break;
    case A_I2F:       printf("(i2f "); dump_expr(e->a); printf(")"); break;
    case A_BINOP:     printf("(binop %s ", op_str(e->op));
                      dump_expr(e->a); printf(" ");
                      dump_expr(e->b); printf(")"); break;
    case A_ERR:       printf("(error)"); break;
    default:          printf("(?)"); break;
    }
}

static void dump_stmt(const AstNode *s, int ind);

/* 打印一行缩进 + 语句（供链式语句共用） */
static void dump_stmt_line(const AstNode *s, int ind)
{
    printf("%*s", ind, "");
    dump_stmt(s, ind);
    printf("\n");
}

static void dump_stmt(const AstNode *s, int ind)
{
    switch (s->kind) {
    case A_PROG:
        printf("(program\n");
        for (const AstNode *p = s->a; p; p = p->next) dump_stmt_line(p, ind + 2);
        printf("%*s)", ind, "");
        break;
    case A_DECL:
        printf("(decl %s %s", type_name(s->sym ? s->sym->type : TY_ERR), s->name);
        if (s->a) { printf(" "); dump_expr(s->a); }
        printf(")");
        break;
    case A_ASSIGN:
        printf("(assign %s ", s->name);
        dump_expr(s->a); printf(")");
        break;
    case A_PRINT:
        printf("(print "); dump_expr(s->a); printf(")");
        break;
    case A_EXPRSTMT:
        printf("(exprst "); dump_expr(s->a); printf(")");
        break;
    case A_IF:
        printf("(if "); dump_expr(s->a); printf("\n");
        dump_stmt_line(s->b, ind + 2);
        if (s->c) dump_stmt_line(s->c, ind + 2);
        printf("%*s)", ind, "");
        break;
    case A_WHILE:
        printf("(while "); dump_expr(s->a); printf("\n");
        dump_stmt_line(s->b, ind + 2);
        printf("%*s)", ind, "");
        break;
    case A_BLOCK:
        printf("(block\n");
        for (const AstNode *p = s->a; p; p = p->next) dump_stmt_line(p, ind + 2);
        printf("%*s)", ind, "");
        break;
    case A_BREAK:    printf("(break)"); break;
    case A_CONTINUE: printf("(continue)"); break;
    case A_EMPTY:    printf("(empty)"); break;
    default:         printf("(?)"); break;
    }
}

void ast_dump(const AstNode *prog)
{
    dump_stmt(prog, 0);
    printf("\n");
}

/* ================= 释放 =================
 * 语句有 next 兄弟链，表达式有 a/b/c 孩子树——两个小函数互相递归，
 * 对"孩子恰好是语句链头"（block/if/while 的体）也自动正确。 */

static void free_node(AstNode *n);

static void free_chain(AstNode *n)
{
    while (n) {
        AstNode *nx = n->next;
        free_node(n);
        n = nx;
    }
}

static void free_node(AstNode *n)
{
    free_chain(n->a);
    free_chain(n->b);
    free_chain(n->c);
    free(n);   /* 只释放节点本身：name 字符串与 Sym 的生命周期更长（见 ast.h） */
}

void ast_free(AstNode *prog) { free_chain(prog); }
