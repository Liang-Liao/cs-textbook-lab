/*
 * ll_parser.c —— LL(1) 预测分析器（龙书 4.4.2 节图 4.x 的算法直译）
 *
 * 与 rd_parser 的本质区别：**文法不再写死在函数调用结构里**，
 * 而是变成数据（grammar.c 的 PRODS 表）+ 一张自动算出来的分析表 +
 * 一个通用驱动循环。换一门语言只要换文法数据——这就是"语法分析器
 * 生成器"（ANTLR/JavaCC 一族）的内核。
 *
 * 算法（栈顶在右）：
 *   栈=[EOF, 开始符号], 输入指向第一个记号
 *   循环:
 *     top = 栈顶
 *     若 top 是终结符: 与当前记号匹配则双双前进，否则报错
 *     若 top 是非终结符 A: 查 M[A][当前记号]
 *        有产生式 A→α 则弹出 A、逆序压入 α，打印这一步推导
 *        没有则报错
 *     栈空且输入到 EOF: 接受
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ll_parser.h"
#include "lexer.h"
#include "grammar.h"

#define STKCAP 4096
static int stk[STKCAP];
static int top;

/* 压栈前查容量：展开产生式一次净增最多 2 个符号，上千层嵌套
 * 括号即可打满 4096——不设防就是静默越界写，必须明确报错。 */
static void push(int s)
{
    if (top >= STKCAP) {
        fprintf(stderr, "error: 分析栈溢出（嵌套过深，上限 %d）\n", STKCAP);
        exit(1);
    }
    stk[top++] = s;
}
static int  pop(void)   { return stk[--top]; }

/* 打印一行 trace: 栈（左底右顶）| 剩余输入 | 动作 */
static void dump_stack(void)
{
    for (int i = 0; i < top; i++) {
        int s = stk[i];
        if (s >= NT_BASE) printf("%s", sym_name(s));
        else if (s == EPS) printf("ε");
        else printf("%s", token_name(s) + 2);
        if (i != top - 1) putchar(' ');
    }
}

int ll_parse(const char *src, int trace)
{
    Lexer lx;
    lexer_init(&lx, src);
    Token cur = lexer_next_clean(&lx);
    int nerr = 0;
    int steps = 0;

    top = 0;
    push(T_EOF);
    push(NT_BASE + NT_STMT_LIST);

    while (top > 0) {
        steps++;
        int A = stk[top - 1];

        if (trace) {
            printf("%-3d | ", steps);
            dump_stack();
            printf(" | %-10s | ", cur.lexeme);
        }

        /* ---- 栈顶是终结符（含 EOF）---- */
        if (A < NT_BASE) {
            if (A == (int)cur.type) {
                if (trace) printf("匹配 %s\n", cur.lexeme);
                pop();
                cur = lexer_next_clean(&lx);
            } else {
                if (trace) printf("\n");
                fprintf(stderr,
                    "line %d, col %d: error: 语法错误: 意外的 %s"
                    "（期望 %s）\n",
                    cur.line, cur.col, cur.lexeme, token_name(A));
                nerr++;
                /* 恢复：弹出期望的终结符，视为"插入"了它 */
                pop();
                if (nerr > 20) return 1;   /* 防失控 */
            }
            continue;
        }

        /* ---- 栈顶是非终结符：查表 ---- */
        int p = LL_TABLE[A - NT_BASE][cur.type];
        if (p == -1) {
            if (trace) printf("\n");
            fprintf(stderr,
                "line %d, col %d: error: 语法错误: %s 后面不能接 %s\n",
                cur.line, cur.col, sym_name(A), cur.lexeme);
            nerr++;
            /* ==== panic 模式错误恢复（龙书 4.4.3）====
             * 弹掉 A；然后丢弃输入记号，直到遇到 FOLLOW(A) 里的
             * 记号（同步记号）为止——句子在 A 结束的地方重新同步。
             * 这是最朴素但极有效的恢复策略，现代编译器用它变体。 */
            int a = A - NT_BASE;
            pop();
            while (cur.type != T_EOF &&
                   !(FOLLOW_SET[a] >> cur.type & 1)) {
                if (trace)
                    fprintf(stderr, "  [panic] 丢弃 %s\n", cur.lexeme);
                cur = lexer_next_clean(&lx);
            }
            if (nerr > 20) return 1;
            continue;
        }

        /* 展开产生式 P#: A → rhs */
        if (trace) {
            Prod *pr = &PRODS[p];
            printf("P%-2d: %s ->", p, sym_name(pr->lhs));
            for (int i = 0; i < pr->n; i++)
                printf(" %s", sym_name(pr->rhs[i]));
            printf("\n");
        }
        pop();
        Prod *pr = &PRODS[p];
        for (int i = pr->n - 1; i >= 0; i--)
            if (pr->rhs[i] != EPS)
                push(pr->rhs[i]);          /* ε 不入栈（展开即消隐） */
    }

    /* 栈空了。输入也该到 EOF */
    if (cur.type != T_EOF) {
        fprintf(stderr, "line %d, col %d: error: 语法错误: "
                "语句结束后还有内容 %s\n",
                cur.line, cur.col, cur.lexeme);
        nerr++;
    }

    if (trace) printf("共 %d 步, %s\n", steps, nerr ? "reject" : "accept");
    return nerr ? 1 : 0;
}
