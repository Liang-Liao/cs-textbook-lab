/*
 * grammar.h —— MiniC v2 文法（数据形式）+ FIRST/FOLLOW + LL(1) 分析表
 *              （lab3，对照龙书 4.3~4.4 节）
 */
#ifndef GRAMMAR_H
#define GRAMMAR_H

#include <stdint.h>
#include "token.h"

/* ============ 符号编码 ============
 * 文法符号 = 终结符 | 非终结符 | ε
 *   终结符:  直接复用 TokenType 的枚举值（0..T_ERROR）
 *   非终结符: NT_BASE + 序号
 *   ε:      EPS（特殊值，只出现在产生式右部）
 */
#define EPS      (-2)
#define NT_BASE  100

enum NonTerm {
    NT_STMT_LIST,   /* 语句序列（本 lab 的开始符号） */
    NT_STMT,        /* 语句: print expr; | expr; */
    NT_EXPR,        /* 表达式 */
    NT_EQUALITY, NT_EQUALITY_P,  /* == != 层及它的"尾巴" */
    NT_REL,     NT_REL_P,        /* < <= > >= 层 */
    NT_ADD,     NT_ADD_P,        /* + - 层 */
    NT_TERM,    NT_TERM_P,       /* * / % 层 */
    NT_FACTOR,                      /* 一元/原子层 */
    NT_COUNT
};

typedef struct {
    int  lhs;          /* 左部（非终结符编码） */
    int  rhs[5];       /* 右部符号序列 */
    int  n;            /* 右部长度（ε 产生式记为 n=1, rhs[0]=EPS） */
    int  id;           /* 产生式编号（数组下标） */
} Prod;

/* 产生式表（顺序即编号）。这是"文法即数据"的核心——LL(1) 分析表、
 * FIRST/FOLLOW 全部由它自动计算，不靠手写。 */
extern Prod  PRODS[];
extern int   NPRODS;

/* 文法符号的名字（打印用；终结符用 token 名去掉 T_ 前缀更清爽） */
const char *sym_name(int sym);

/* ---------------- FIRST / FOLLOW ----------------
 * 用 64 位位集表示"终结符集合"（终结符编号 < 47，一个 uint64 足够）。 */
typedef uint64_t TSet;

extern TSet FIRST_SET[NT_COUNT];   /* 非终结符的 FIRST（不含 ε，可空性单独记） */
extern int  NULLABLE[NT_COUNT];    /* 能否推出 ε */
extern TSet FOLLOW_SET[NT_COUNT];  /* 非终结符的 FOLLOW */

void compute_first_follow(void);   /* 不动点迭代，一次性算完（main 里先调用） */

/* 便捷：求符号序列 alpha 的 FIRST（带"是否突然中断"语义），
 * 用于造表。alpha 的 FIRST 若含"可空前缀"会继续往后看。 */
TSet seq_first(const int *syms, int n, int *nullable_out);

/* ---------------- LL(1) 分析表 ----------------
 * M[非终结符][终结符] = 产生式编号，-1 表示空白（出错）。
 * 表构造（龙书 4.4.2 节）：
 *   对 A→α: FIRST(α) 里每个 a 填 M[A,a]=A→α；
 *           若 α 可空，FOLLOW(A) 里每个 b 再填 M[A,b]=A→α。
 * 同一格被填两次 = 冲突 = 文法不是 LL(1)。 */
extern int LL_TABLE[NT_COUNT][T_ERROR + 1];

void build_ll_table(void);         /* 无返回值；冲突数经 ll_conflicts() 查询 */
int   ll_conflicts(void);

/* 打印工具（-ff / -table 选项） */
void print_first_follow(void);
void print_ll_table(void);

#endif /* GRAMMAR_H */
