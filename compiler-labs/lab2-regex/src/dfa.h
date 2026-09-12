/*
 * dfa.h —— 子集构造、DFA 最小化、DFA 模拟（lab2）
 */
#ifndef DFA_H
#define DFA_H

#include <stdint.h>
#include "nfa.h"

/* DFA：转移表 trans[state*256 + 字符] = 目标状态（-1 表示死路）。
 * acc[state] = 接受时的规则编号（0 = 不接受；多模式时是最小规则号，
 * 体现"规则写在前面优先级高"的 flex 语义）。 */
typedef struct {
    int  n;
    int  start;
    int *trans;   /* n * 256 */
    int *acc;
} DFA;

/* 子集构造（对照龙书 3.7 节"从 NFA 到 DFA"，即著名的算法 3.20）
 * 返回新建的 DFA。 */
DFA dfa_from_nfa(const NFA *nfa);

/* DFA 最小化（对照龙书 3.9 节：按"接受标记 + 转移等价"反复划分）。
 * 这里实现等价于 Hopcroft 思想的朴素划分细化(Moore)版本：
 * 初始按 acc 分组，然后反复用"对每个字符到达同一组"来细化，
 * 直到不再变化。O(n^2 * |Σ|)，状态数小，教学上足够清晰。 */
DFA dfa_minimize(const DFA *d);

/* 用 DFA 判断"整串是否匹配"（-scan 模式不用这个，用最长匹配） */
int dfa_accept_all(const DFA *d, const char *s);

/* 输出最小 DFA 的 DOT 图 */
void dfa_dump_dot(const DFA *d, const char *title);

#endif /* DFA_H */
