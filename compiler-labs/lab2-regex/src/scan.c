#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "re_parse.h"
#include "nfa.h"
#include "dfa.h"
#include "scan.h"

#define MAXRULES 64
#define MAXLINE  512

static char *read_all(const char *path, long *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "error: 无法打开 %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    *out_len = (long)fread(buf, 1, (size_t)n, f);
    buf[*out_len] = '\0';
    fclose(f);
    return buf;
}

static char *rtrim(char *s)
{
    size_t n = strlen(s);
    while (n && (s[n-1] == '\n' || s[n-1] == '\r' ||
                 s[n-1] == ' '  || s[n-1] == '\t'))
        s[--n] = '\0';
    return s;
}

void scan_file(const char *rules_path, const char *input_path)
{
    /* ---------- 1. 读规则文件 ---------- */
    FILE *rf = fopen(rules_path, "r");
    if (!rf) { fprintf(stderr, "error: 无法打开规则文件 %s\n", rules_path); exit(1); }

    char *names[MAXRULES];
    Node *asts[MAXRULES];
    int nrules = 0;
    char rulebuf[MAXLINE];
    while (fgets(rulebuf, sizeof rulebuf, rf)) {
        char *p = rulebuf;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\0') continue; /* 注释/空行 */

        /* 名字 = 第一个空白前的词；正则 = 其余部分（可含空格类如 [ ]，但
         * 首尾空白已被我们吃掉，规则书写时注意别让正则以空格开头） */
        char *name = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
        if (*p == '\0' || *p == '\n') {
            fprintf(stderr, "error: 规则缺正则部分: %s", name);
            exit(1);
        }
        *p++ = '\0';
        while (*p == ' ' || *p == '\t') p++;
        rtrim(p);
        if (*p == '\0') { fprintf(stderr, "error: 规则缺正则部分: %s\n", name); exit(1); }

        Node *ast = re_parse(p);
        if (!ast) { fprintf(stderr, "  (在规则 %s 里)\n", name); exit(1); }

        if (nrules >= MAXRULES) {
            fprintf(stderr, "error: 规则数超过上限 %d（文件 %s）\n",
                    MAXRULES, rules_path);
            exit(1);
        }
        names[nrules] = strdup(name);
        asts[nrules] = ast;
        nrules++;
    }
    fclose(rf);

    /* ---------- 2. 编译：多模式并联 NFA → 子集构造 → 最小化 ---------- */
    NFA nfa;
    nfa_init(&nfa);
    nfa_build_multi(&nfa, asts, nrules);

    DFA dfa = dfa_from_nfa(&nfa);
    DFA min = dfa_minimize(&dfa);
    fprintf(stderr, "[stats] rules=%d  NFA states=%d  DFA states=%d  "
            "minimized=%d\n", nrules, nfa.n, dfa.n, min.n);

    /* ---------- 3. 最长匹配扫描 ---------- */
    long len;
    char *src = read_all(input_path, &len);
    long pos = 0;
    int line = 1, col = 1;   /* 行列语义与 lab1 一致：字节计数 */
    int nerrors = 0;
    /* lab1 的 EOF 记号位置是"最后一个 token 之后"（跳尾随空白前），
     * 为了对拍一致，这里单独记录 */
    int lastline = 1, lastcol = 1;

    while (pos < len) {
        /* 记号起点（跳过空白后由最长匹配自然处理——空白规则是 SKIP） */
        int tline = line, tcol = col;

        int st = min.start;
        int last_len = -1, last_tag = 0;   /* 迄今最长接受的长度与规则 */
        long i = pos;
        while (i < len) {
            unsigned char c = (unsigned char)src[i];
            st = min.trans[st * 256 + c];
            if (st < 0) break;             /* 死路：最长匹配到此为止 */
            i++;
            if (min.acc[st]) { last_len = (int)(i - pos); last_tag = min.acc[st]; }
        }

        if (last_len < 0) {
            /* 没有任何规则能从当前位置吃进一个字符：词法错误 */
            fprintf(stderr, "line %d, col %d: error: 无法识别的字符 '%c'\n",
                    tline, tcol, src[pos]);
            nerrors++;
            pos++;                        /* 恢复：跳一个字节继续 */
            if (src[pos - 1] == '\n') { line++; col = 1; } else col++;
            continue;
        }

        /* 输出词素（SKIP 除外），并推进行列计数 */
        if (strcmp(names[last_tag - 1], "SKIP") != 0) {
            printf("%d:%d\t%-12s\t%.*s\n", tline, tcol,
                   names[last_tag - 1], last_len, src + pos);
        }
        for (long k = 0; k < last_len; k++) {
            if (src[pos + k] == '\n') { line++; col = 1; }
            else col++;
        }
        if (strcmp(names[last_tag - 1], "SKIP") != 0) {
            lastline = line; lastcol = col;
        }
        pos += last_len;
    }
    printf("%d:%d\tT_EOF\t\n", lastline, lastcol);
    exit(nerrors ? 1 : 0);
}
