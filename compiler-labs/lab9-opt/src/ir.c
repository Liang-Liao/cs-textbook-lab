/*
 * ir.c —— 四元式的文本打印（对照龙书 6.2 节）
 *
 * 输出形如（-ir 开关）：
 *
 *   func fact(n):
 *   L1:
 *     t1 = n <= 1
 *     if t1 goto L2        ← gen_cond 数值真值走 if_false，比较走直跳
 *     ...
 *   main:
 *     t9 = call fact, 1
 *     print t9
 *
 * 这份文本本身就是"对拍 IR"的期望文件——生成器改了翻译策略，
 * diff 一眼可见（tests/cases/ir.expected）。
 */
#include <stdio.h>
#include "ir.h"

/* 比较运算符的可打印形式（自包含，不依赖 ast.c 的同名 static 函数） */
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

static void print_quad(const Quad *q)
{
    switch (q->op) {
    case Q_LABEL:   printf("L%d:\n", q->label); break;
    case Q_GOTO:    printf("  goto L%d\n", q->label); break;
    case Q_IF_GOTO: printf("  if %s %s %s goto L%d\n",
                           q->y, op_str(q->bop), q->z, q->label); break;
    case Q_IFF_GOTO:printf("  if_false %s goto L%d\n", q->y, q->label); break;
    case Q_ASSIGN:  printf("  %s = %s\n", q->x, q->y); break;
    case Q_BINOP:   printf("  %s = %s %s %s\n",
                           q->x, q->y, op_str(q->bop), q->z); break;
    case Q_NEG:     printf("  %s = -%s\n", q->x, q->y); break;
    case Q_NOT:     printf("  %s = !%s\n", q->x, q->y); break;
    case Q_I2F:     printf("  %s = (float)%s\n", q->x, q->y); break;
    case Q_LDX:     printf("  %s = %s[%s]\n", q->x, q->y, q->z); break;
    case Q_STX:     printf("  %s[%s] = %s\n", q->y, q->z, q->x); break;
    case Q_PARAM:   printf("  param %s\n", q->y); break;
    case Q_CALL:    if (q->x) printf("  %s = call %s, %d\n",
                                     q->x, q->y, q->argc);
                    else      printf("  call %s, %d\n", q->y, q->argc);
                    break;
    case Q_RETURN:  if (q->y) printf("  return %s\n", q->y);
                    else      printf("  return\n");
                    break;
    case Q_PRINT:   printf("  print %s\n", q->y); break;
    default:        printf("  ?\n"); break;
    }
}

void ir_print(const IrProgram *p)
{
    for (int i = 0; i < p->nfuncs; i++) {
        const IrFunc *f = &p->funcs[i];
        if (i > 0) printf("\n");           /* 段间空行 */
        printf("%s(", f->name);
        for (int j = 0; j < f->nparams; j++)
            printf("%s%s", j ? ", " : "", f->params[j]);
        printf("):\n");
        for (int j = 0; j < f->n; j++)
            print_quad(&f->q[j]);
    }
}
