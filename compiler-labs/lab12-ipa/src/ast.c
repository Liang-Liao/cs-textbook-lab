/*
 * ast.c —— AST 节点的构造、S-表达式 dump、释放（对照龙书 5.1/5.3 节）
 *
 * 在 lab5 基础上加了六种节点（短路布尔三种、调用/返回/函数定义）。
 * dump 仍是 Lisp 风格 S-表达式，例如：
 *   (program
 *     (func int add(int a, int b)
 *       (block (return (binop + (var a) (var b))))
 *     )
 *     (print (call add (int 1) (i2f (int 2))))
 *   )
 */
#include <stdio.h>
#include <stdlib.h>
#include "ast.h"

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
    n->type = TY_INT;
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
    n->type = sym->type;
    n->name = sym->name;
    n->sym  = sym;
    return n;
}

/* a[i]：类型 = 元素类型（Sym->type 存的就是元素类型）；sym 挂数组符号，
 * a = 下标表达式。下标的合法性检查在 parser 完成。 */
AstNode *ast_index(Sym *arr, AstNode *idx, int line)
{
    AstNode *n = ast_new(A_INDEX, line);
    n->type = arr ? arr->type : TY_ERR;
    n->name = arr ? arr->name : "?";
    n->sym  = arr;
    n->a    = idx;
    return n;
}

/* lab11：m[i][j]——a=行下标、c=列下标（槽位约定见 ast.h） */
AstNode *ast_index2(Sym *arr, AstNode *row, AstNode *col, int line)
{
    AstNode *n = ast_index(arr, row, line);
    n->c = col;
    return n;
}

AstNode *ast_neg(AstNode *a, int line)
{
    AstNode *n = ast_new(A_NEG, line);
    n->type = a->type;
    n->a = a;
    return n;
}

AstNode *ast_not(AstNode *a, int line)   /* !e：操作数 int/float，结果 int */
{
    AstNode *n = ast_new(A_NOT, line);
    n->type = TY_INT;
    n->a = a;
    return n;
}

AstNode *ast_binop(TokenType op, AstNode *a, AstNode *b, Type t, int line)
{
    AstNode *n = ast_new(A_BINOP, line);
    n->type = t;
    n->op = op;
    n->a = a;
    n->b = b;
    return n;
}

/* && 与 || 共用：短路布尔不是普通 BINOP——求值顺序有讲究（右操作数
 * 可能被跳过），IR 翻译走完全不同的路径（6.6 节的跳转代码 + 回填） */
AstNode *ast_logic(AstKind and_or, AstNode *a, AstNode *b, int line)
{
    AstNode *n = ast_new(and_or, line);   /* 调用方保证 A_AND 或 A_OR */
    n->type = TY_INT;                     /* 结果总是 int 0/1 */
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

AstNode *ast_call(Sym *sym, AstNode *first_arg, int line)
{
    AstNode *n = ast_new(A_CALL, line);
    n->type = sym->type;                  /* 调用表达式的类型 = 返回类型 */
    n->name = sym->name;
    n->sym  = sym;
    n->a    = first_arg;                  /* 实参沿 next 串成链 */
    return n;
}

AstNode *ast_err(int line)
{
    AstNode *n = ast_new(A_ERR, line);
    n->type = TY_ERR;
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
    if (sym) n->name = sym->name;
    n->sym  = sym;
    n->a    = init;
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

/* a[i] = e：sym 挂数组符号，a = 下标，b = 右值（parser 已按元素类型
 * 对右值做过赋值式检查） */
AstNode *ast_store(Sym *arr, AstNode *idx, AstNode *rhs, int line)
{
    AstNode *n = ast_new(A_STORE, line);
    if (arr) n->name = arr->name;
    n->sym = arr;
    n->a   = idx;
    n->b   = rhs;
    return n;
}

/* lab11：m[i][j] = e——a=行下标、c=列下标、b=右值 */
AstNode *ast_store2(Sym *arr, AstNode *row, AstNode *col, AstNode *rhs,
                    int line)
{
    AstNode *n = ast_store(arr, row, rhs, line);
    n->c = col;
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
    n->c = el;
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

AstNode *ast_return(AstNode *e, int line)
{
    AstNode *n = ast_new(A_RETURN, line);
    n->a = e;
    return n;
}

AstNode *ast_funcdef(Sym *sym, AstNode *body,
                     Sym **params, int nparams, int line)
{
    AstNode *n = ast_new(A_FUNCDEF, line);
    n->name = sym->name;
    n->sym  = sym;                        /* 签名（返回类型/参数类型）都在这里 */
    n->a    = body;                       /* A_BLOCK：参数与局部同在其作用域 */
    for (int i = 0; i < nparams; i++)     /* 形参符号（绑定实参用） */
        n->psym[i] = params[i];
    sym->fdef = n;                        /* 符号 → 定义节点（调用点跳转用） */
    return n;
}

/* ================= dump ================= */

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

static void dump_expr(const AstNode *e)
{
    switch (e->kind) {
    case A_INT_LIT:   printf("(int %ld)", e->ival); break;
    case A_FLOAT_LIT: printf("(float %g)", e->dval); break;
    case A_VAR:       printf("(var %s)", e->name); break;
    case A_INDEX:
        printf("(index %s[", e->name);
        dump_expr(e->a); printf("]");
        if (e->c) { printf("["); dump_expr(e->c); printf("]"); }
        printf(")");
        break;
    case A_NEG:       printf("(neg "); dump_expr(e->a); printf(")"); break;
    case A_NOT:       printf("(not "); dump_expr(e->a); printf(")"); break;
    case A_I2F:       printf("(i2f "); dump_expr(e->a); printf(")"); break;
    case A_AND:       printf("(and "); dump_expr(e->a); printf(" ");
                      dump_expr(e->b); printf(")"); break;
    case A_OR:        printf("(or ");  dump_expr(e->a); printf(" ");
                      dump_expr(e->b); printf(")"); break;
    case A_BINOP:     printf("(binop %s ", op_str(e->op));
                      dump_expr(e->a); printf(" ");
                      dump_expr(e->b); printf(")"); break;
    case A_CALL:      printf("(call %s", e->name);
                      for (const AstNode *p = e->a; p; p = p->next) {
                          printf(" "); dump_expr(p);
                      }
                      printf(")"); break;
    case A_ERR:       printf("(error)"); break;
    default:          printf("(?)"); break;
    }
}

static void dump_stmt(const AstNode *s, int ind);

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
        if (s->sym && s->sym->arr_len > 0) {   /* 数组：类型打印成 int[8] */
            printf("(decl %s[%d]", type_name(s->sym->type), s->sym->arr_len);
            if (s->sym->arr_dim2 > 0) printf("[%d]", s->sym->arr_dim2);
            printf(" %s)", s->name);
        } else {
            printf("(decl %s %s",
                   type_name(s->sym ? s->sym->type : TY_ERR), s->name);
            if (s->a) { printf(" "); dump_expr(s->a); }
            printf(")");
        }
        break;
    case A_ASSIGN:
        printf("(assign %s ", s->name);
        dump_expr(s->a); printf(")");
        break;
    case A_STORE:
        printf("(store %s[", s->name);
        dump_expr(s->a);
        if (s->c) { printf("]["); dump_expr(s->c); }
        printf("] ");
        dump_expr(s->b); printf(")");
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
    case A_RETURN:
        if (s->a) { printf("(return "); dump_expr(s->a); printf(")"); }
        else        printf("(return)");
        break;
    case A_FUNCDEF: {   /* 签名从符号表条目打印：返回类型 + 参数表 */
        const Sym *f = s->sym;
        printf("(func %s %s(", type_name(f->type), s->name);
        for (int i = 0; i < f->nparams; i++)
            printf("%s%s", i ? ", " : "", type_name(f->ptypes[i]));
        printf(")\n");
        dump_stmt_line(s->a, ind + 2);   /* 函数体（A_BLOCK） */
        printf("%*s)", ind, "");
        break;
    }
    default:         printf("(?)"); break;
    }
}

void ast_dump(const AstNode *prog)
{
    dump_stmt(prog, 0);
    printf("\n");
}

/* ================= 释放（同 lab5：链 + 孩子互相递归） ================= */

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
    free(n);
}

void ast_free(AstNode *prog) { free_chain(prog); }
