/*
 * slr.h —— LR(0) 项目集规范族 + SLR(1) 分析表 + 表驱动分析器
 *           （lab4，对照龙书 4.5~4.6 节）
 */
#ifndef SLR_H
#define SLR_H

/* 构造 LR(0) 自动机（规范项目集族）与 SLR(1) 表。
 * shift/reduce 冲突（本文法恰好 1 个：悬空 else）按 yacc 惯例
 * "优先移进"消解并打印说明。reduce/reduce 冲突属于文法真错误、
 * 无惯例可消解：由调用方查 slr_num_rr_conflicts() > 0 后拒绝
 * （main.c 的 build_and_check 统一执行这一契约）。 */
void slr_build(void);

int slr_num_states(void);
int slr_num_conflicts(void);
int slr_num_sr_conflicts(void);
int slr_num_rr_conflicts(void);

/* 打印工具 */
void slr_print_states(void);
void slr_print_table(void);

/* 表驱动分析。trace=1 打印每步：状态栈 | 符号栈 | 动作。
 * 返回 0=接受 1=拒绝。 */
int slr_parse(const char *src, int trace);

#endif /* SLR_H */
