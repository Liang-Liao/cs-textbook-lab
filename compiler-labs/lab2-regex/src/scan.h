/*
 * scan.h —— mini 词法生成器：把"多条正则规则"编译成一个 DFA，
 *           再用最长匹配策略做词法扫描（lab2，这正是 flex 的原理）
 */
#ifndef SCAN_H
#define SCAN_H

/* 用规则文件对输入做词法扫描。
 * 规则文件格式（每行一条，# 开头是注释）：
 *     记号名  正则
 * 特殊记号名 SKIP 表示匹配后丢弃（用于空白和注释）。
 *
 * 词法歧义的两条 flex 语义（龙书 3.5 节）：
 *   1. 最长匹配优先：iffy 更长地匹配 T_IDENT 而不是 T_KW_IF；
 *   2. 等长时规则靠前优先：if 同时匹配 T_KW_IF 和 T_IDENT(等长)，
 *      取先写的 T_KW_IF——这就是"关键字规则写在标识符前"的原因。
 *
 * 输出格式与 lab1 的 minilex 完全一致（对拍用）：
 *     行:列<TAB>类型<TAB>词素
 * 统计信息打到 stderr。
 */
void scan_file(const char *rules_path, const char *input_path);

#endif /* SCAN_H */
