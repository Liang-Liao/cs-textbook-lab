/*
 * main.c —— lab2 驱动：正则引擎 / mini 词法生成器
 *
 * 用法:
 *   ./minire.exe <正则> <字符串>...    测试整串匹配（打印 accept/reject）
 *   ./minire.exe -dot <正则>           输出 NFA 和最小 DFA 的 DOT 图
 *   ./minire.exe -scan <规则文件> <输入文件>   用自动生成的 DFA 做词法扫描
 *
 * 例子:
 *   ./minire.exe '(a|b)*abb' abb abbb bab
 *   ./minire.exe -dot '(a|b)*abb' > dfa.dot    # 有 graphviz 可: dot -Tsvg
 *   ./minire.exe -scan tests/cases/minic.rules tests/cases/demo.mc
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "re_parse.h"
#include "nfa.h"
#include "dfa.h"
#include "scan.h"

int main(int argc, char **argv)
{
    /* ---- -scan 模式：mini-flex ---- */
    if (argc >= 2 && strcmp(argv[1], "-scan") == 0) {
        if (argc != 4) {
            fprintf(stderr, "用法: %s -scan <规则文件> <输入文件>\n", argv[0]);
            return 1;
        }
        scan_file(argv[2], argv[3]);
        return 0; /* scan_file 内部 exit */
    }

    /* ---- -dot 模式：可视化自动机 ---- */
    int dot = 0;
    int argi = 1;
    if (argi < argc && strcmp(argv[argi], "-dot") == 0) { dot = 1; argi++; }

    if (argi >= argc) {
        fprintf(stderr,
            "用法:\n"
            "  %s <正则> <字符串>...           测试匹配\n"
            "  %s -dot <正则>                  输出 DOT 图\n"
            "  %s -scan <规则文件> <输入文件>  mini-flex 词法扫描\n",
            argv[0], argv[0], argv[0]);
        return 1;
    }

    /* ---- 单模式：正则 → NFA → DFA → 最小化 → 测试 ---- */
    Node *ast = re_parse(argv[argi]);
    if (!ast) return 1;
    argi++;

    NFA nfa;
    nfa_init(&nfa);
    nfa_build(&nfa, ast);

    DFA dfa = dfa_from_nfa(&nfa);
    DFA min = dfa_minimize(&dfa);

    if (dot) {
        printf("/* NFA (Thompson 构造) */\n");
        nfa_dump_dot(&nfa, "NFA");
        printf("\n/* 最小化 DFA */\n");
        dfa_dump_dot(&min, "minDFA");
        return 0;
    }

    fprintf(stderr, "[stats] NFA states=%d  DFA states=%d  minimized=%d\n",
            nfa.n, dfa.n, min.n);

    /* 对每个参数串做整串匹配测试（龙书 3.8 节模拟 DFA 的用法） */
    int all_ok = 1;
    for (int i = argi; i < argc; i++) {
        int ok = dfa_accept_all(&min, argv[i]);
        printf("%-20s %s\n", argv[i], ok ? "accept" : "reject");
        if (!ok) all_ok = 0;
    }
    return all_ok ? 0 : 1;   /* 有 reject 即退出码 1，脚本可据此判定 */
}
